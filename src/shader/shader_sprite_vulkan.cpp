#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_sprite.h"
// clang-format on

#include "btdatecheck.h"
#include "btglm.h"
#include "btlogger.h"
#include "renderer/gfx.h"
#include "renderer/gfx_vulkan/vk_buffer.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader/shader_support.h"
#include "shader_creation/shader_creation.h"
#include "txp_renderer/ui/ui_state.h"
#include "ui/ui_types.h"
#include "vulkan/vulkan_core.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace Font
{

struct Font_file_atlas_data
{
    std::string type;  // only MSDF support currently!
    float_t distanceRange;
    float_t distanceRangeMiddle;
    float_t size;
    uint32_t width;
    uint32_t height;
    std::string yOrigin;
};

struct Font_file_metrics_data
{
    float_t emSize;
    float_t lineHeight;
    float_t ascender;
    float_t descender;
    float_t underlineY;
    float_t underlineThickness;
};

struct Font_file_glyphs_data
{
    uint32_t unicode;
    float_t advance;

    struct Bounds
    {
        float_t left{ std::numeric_limits<float_t>::lowest() };
        float_t right{ std::numeric_limits<float_t>::lowest() };
        float_t top{ std::numeric_limits<float_t>::lowest() };
        float_t bottom{ std::numeric_limits<float_t>::lowest() };
    };

    Bounds planeBounds;  // optional
    Bounds atlasBounds;  // optional
};

struct Font_file_kerning_data
{
    // idk what this data looks like right now
};

struct Font_file_data
{
    Font_file_atlas_data atlas;
    Font_file_metrics_data metrics;
    std::vector<Font_file_glyphs_data> glyphs;
    std::vector<Font_file_kerning_data> kerning;
};

} // namespace Font

namespace Shader
{

/// Struct for push constants.
struct Shader_sprite_push_constants  // @TODO: move this to gfx_vulkan_impl!!!  @AMEND: what is this message????????
{
    VkDeviceAddress sprite_data_set_dev_addr;
    float_t camera_view_aspect_ratio;
    float_t render_canvas_height;
};

/// Vertex for sprites.
struct Sprite_vertex
{
    vec3 position;
    vec2 uv;
};

/// Structure to organize mesh to upload.
struct Sprite_mesh_to_upload
{
    std::vector<Sprite_vertex> vertices;
    std::vector<uint32_t> indices;
};

struct Uploaded_mesh
{
    bool is_volatile;
    std::unique_ptr<Graphics::Impl::Model_buffer> model_buffer;
    uint32_t index_count;
    bool got_used;
};


// struct Shader_sprite::Impl
struct Shader_sprite::Impl
{
    Impl(TXP::Render_model_data_collection& rend_mod_data_coll, TXP::Graphics::Impl& graphics)
        : render_model_data_collection(rend_mod_data_coll)
        , g(graphics)
        , device(g.gfx.device)
        , allocator(g.gfx.allocator)
    {
        Shader_Support::fetch_graphics_shader_info(k_name,
                                                   vertex_entry_point_name,
                                                   fragment_entry_point_name);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                 .size =
                                                     sizeof(Shader_sprite_push_constants) };
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &g.all_textures_descriptor_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
        };

        err = vkCreatePipelineLayout(device,
                                     &pipeline_layout_info,
                                     nullptr,
                                     &shader_pipeline.pipeline_layout);
        if (err)
            throw std::runtime_error("Failed to create pipeline layout.");

        // Create pipeline.
        VkVertexInputBindingDescription vertex_binding{ .binding = 0,
                                                        .stride = sizeof(Sprite_vertex),
                                                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, };
        std::vector<VkVertexInputAttributeDescription> vertex_attributes{
            { .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Sprite_vertex, position) },
            { .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Sprite_vertex, uv) },
        };
        VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertex_binding,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size()),
            .pVertexAttributeDescriptions = vertex_attributes.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        VkShaderModule shader_module{ g.load_shader_module(
            Shader_Creation::get_shader_module_path(k_name)) };

        std::vector<VkPipelineShaderStageCreateInfo> stage_infos;
        stage_infos.reserve(2);

        stage_infos.emplace_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_module,
            .pName = vertex_entry_point_name.c_str(),
            // .pSpecializationInfo = nullptr,  // @RESEARCH: research this if you want!
        });
        stage_infos.emplace_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shader_module,
            .pName = fragment_entry_point_name.c_str(),
            // .pSpecializationInfo = nullptr,  // @RESEARCH: research this if you want!
        });

        VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        std::vector<VkDynamicState> dynamic_states{ VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        std::vector<VkFormat> color_attachment_formats{
            g.ui_image.get_format(),
        };
        VkPipelineRenderingCreateInfo dynamic_rendering_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
            .pColorAttachmentFormats = color_attachment_formats.data(),
        };

        VkPipelineColorBlendAttachmentState blend_attachment{
            // @THEA: all this is different below vv !!! (for color alpha blending)
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 0xf,
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        VkPipelineRasterizationStateCreateInfo rasterization_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .lineWidth = 1.0f,
        };
        VkPipelineMultisampleStateCreateInfo multisample_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkGraphicsPipelineCreateInfo graphics_pipeline_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &dynamic_rendering_info,
            .stageCount = static_cast<uint32_t>(stage_infos.size()),
            .pStages = stage_infos.data(),
            .pVertexInputState = &vertex_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = shader_pipeline.pipeline_layout,
        };

        err = vkCreateGraphicsPipelines(device,
                                        VK_NULL_HANDLE,
                                        1,
                                        &graphics_pipeline_info,
                                        nullptr,
                                        &shader_pipeline.pipeline);
        if (err)
            throw std::runtime_error("Failed to create graphics pipeline.");

        // Cleanup.
        vkDestroyShaderModule(device, shader_module, nullptr);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Create unit square mesh.
        Sprite_mesh_to_upload smesh;

        smesh.vertices.reserve(4);
        smesh.vertices.emplace_back(Sprite_vertex{ .position = { 0, 0, 0 }, .uv = { 0, 0 } });
        smesh.vertices.emplace_back(Sprite_vertex{ .position = { 0, 1, 0 }, .uv = { 0, 1 } });
        smesh.vertices.emplace_back(Sprite_vertex{ .position = { 1, 1, 0 }, .uv = { 1, 1 } });
        smesh.vertices.emplace_back(Sprite_vertex{ .position = { 1, 0, 0 }, .uv = { 1, 0 } });

        smesh.indices.reserve(6);
        smesh.indices.emplace_back(0);
        smesh.indices.emplace_back(1);
        smesh.indices.emplace_back(2);
        smesh.indices.emplace_back(0);
        smesh.indices.emplace_back(2);
        smesh.indices.emplace_back(3);

        upload_mesh_to_gpu(nullptr, std::move(smesh), false);
    }

    ~Impl()
    {
        vkDestroyPipelineLayout(device, shader_pipeline.pipeline_layout, nullptr);
        vkDestroyPipeline(device, shader_pipeline.pipeline, nullptr);
    }


    void upload_mesh_to_gpu(void* ptr_key, Sprite_mesh_to_upload&& smesh, bool is_volatile)
    {
        Uploaded_mesh umesh{ .is_volatile = is_volatile };

        // @COPYPASTA of `upload_model_entries_to_gpu()`
        VkDeviceSize const vertex_buf_size{ sizeof(Sprite_vertex) * smesh.vertices.size() };
        VkDeviceSize const index_buf_size{ sizeof(uint32_t) * smesh.indices.size() };

        VkDeviceSize const total_vertex_index_buffer_size{ vertex_buf_size + index_buf_size };

        umesh.model_buffer = std::make_unique<Graphics::Impl::Model_buffer>();

        umesh.model_buffer->vertex_index_buffer.create(
            g.gfx.device,
            g.gfx.allocator,
            total_vertex_index_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        umesh.model_buffer->offset_to_index_buffer = vertex_buf_size;
        umesh.index_count = smesh.indices.size();

        void* p_mapped_data{ umesh.model_buffer->vertex_index_buffer.get_p_mapped_data() };
        auto const offset_to_idx_buf{ umesh.model_buffer->offset_to_index_buffer };

        std::memcpy(p_mapped_data, smesh.vertices.data(), vertex_buf_size);
        std::memcpy(reinterpret_cast<char*>(p_mapped_data) + offset_to_idx_buf,
                    smesh.indices.data(),
                    index_buf_size);

        // Save to uploaded mesh map.
        pointer_to_uploaded_mesh_map.emplace(ptr_key, std::move(umesh));
    }

    void generate_text(void* ptr_key, std::string const& text)
    {
        auto const& font_texture{ g.texture_entries.at("font_msdf_pitagon_serif_regular").texture };
        assert(font_texture.width == font_texture.height);

        Font::Font_file_data const ui_file_data =
            BT::json_load_from_disk(s_font_directory + "pitagon_serif_regular.btfont");
        assert(ui_file_data.atlas.type == "msdf");
        assert(glm_eq(ui_file_data.atlas.distanceRange, 2));
        assert(glm_eq(ui_file_data.atlas.distanceRangeMiddle, 0));
        assert(ui_file_data.atlas.width == font_texture.width);
        assert(ui_file_data.atlas.height == font_texture.height);

        std::unordered_map<char, Font::Font_file_glyphs_data const*> char_to_glyph_data_map;
        char_to_glyph_data_map.reserve(ui_file_data.glyphs.size());
        for (auto const& glyph : ui_file_data.glyphs)
        {
            assert(glyph.unicode >= 0);
            assert(glyph.unicode < 128);
            char unicode_char = static_cast<char>(glyph.unicode);
            char_to_glyph_data_map[unicode_char] = &glyph;
        }

        Sprite_mesh_to_upload smesh;
        uint32_t index_offset = 0;

        float_t w = static_cast<float_t>(font_texture.width);

        float_t posx = 0.0f;
        float_t posy = 0.0f;

        for (uint32_t i = 0; i < text.size(); i++)
        {
            char char_as_char = text[i];
            auto const& char_info = char_to_glyph_data_map.at(char_as_char);

            if (char_info->width == 0)
                char_info->width = 36;

            float_t charw = ((float_t)(char_info->width) / 36.0f);
            float_t dimx = 1.0f * charw;
            float_t charh = ((float_t)(char_info->height) / 36.0f);
            float_t dimy = 1.0f * charh;

            float_t us = char_info->x / w;
            float_t ue = (char_info->x + char_info->width) / w;
            float_t ts = char_info->y / w;
            float_t te = (char_info->y + char_info->height) / w;

            float_t xo = char_info->xoffset / 36.0f;
            float_t yo = char_info->yoffset / 36.0f;

            posy = yo;

            vertices.push_back({ { posx + dimx + xo, posy + dimy, 0.0f }, { ue, te } });
            vertices.push_back({ { posx + xo, posy + dimy, 0.0f }, { us, te } });
            vertices.push_back({ { posx + xo, posy, 0.0f }, { us, ts } });
            vertices.push_back({ { posx + dimx + xo, posy, 0.0f }, { ue, ts } });

            std::array<uint32_t, 6> letterIndices = { 0, 1, 2, 2, 3, 0 };
            for (auto& index : letterIndices)
            {
                indices.push_back(index_offset + index);
            }
            index_offset += 4;

            float_t advance = ((float_t)(char_info->xadvance) / 36.0f);
            posx += advance;
        }
        index_count = static_cast<uint32_t>(indices.size());

        // Center
        for (auto& v : vertices)
        {
            v.pos[0] -= posx / 2.0f;
            v.pos[1] -= 0.5f;
        }

        // // Generate host accessible buffers for the text vertices and indices and upload the data
        // VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        //                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        //                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        //                                            &vertexBuffer,
        //                                            vertices.size() * sizeof(Vertex),
        //                                            vertices.data()));
        // VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        //                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        //                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        //                                            &indexBuffer,
        //                                            indices.size() * sizeof(uint32_t),
        //                                            indices.data()));
        upload_mesh_to_gpu(ptr_key, std::move(smesh), true);
    }

    void draw_mesh_for_elem(VkCommandBuffer cmd,
                            UI::UI_element const& elem,
                            uint32_t const draw_instance)
    {
        Uploaded_mesh* elem_mesh;
        if (!elem.render_text.empty())
        {
            void* elem_as_ptr{ const_cast<UI::UI_element*>(&elem) };

            auto it{ pointer_to_uploaded_mesh_map.find(elem_as_ptr)};
            if (it == pointer_to_uploaded_mesh_map.end())
            {
                elem_mesh = &generate_text(elem_as_ptr, elem.text);
            }
            else
            {
                elem_mesh = &it->second;
            }
        }
        else
        {
            elem_mesh = &pointer_to_uploaded_mesh_map.at(nullptr);
        }

        elem_mesh->model_buffer->bind(cmd);
        vkCmdDrawIndexed(cmd, elem_mesh->index_count, 1, 0, 0, draw_instance);

        elem_mesh->got_used = true;
    }


    TXP::Render_model_data_collection& render_model_data_collection;

    TXP::Graphics::Impl& g;
    VkDevice device;
    VmaAllocator allocator;

    std::string vertex_entry_point_name;
    std::string fragment_entry_point_name;

    /// Holder for uploaded meshes.
    std::unordered_map<void*, Uploaded_mesh> pointer_to_uploaded_mesh_map;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
    } shader_pipeline;
};


// class Shader_sprite
Shader_sprite::Shader_sprite(Render_model_data_collection& render_model_data_collection,
                             void* graphics)
    : m_pimpl(std::make_unique<Impl>(render_model_data_collection,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_sprite::~Shader_sprite() = default;

void Shader_sprite::upload_ui_state_data(UI_state const& ui_state)
{
    auto render_order_elem_list{ ui_state.gather_rendering_ui_elements_in_render_order() };
    if (render_order_elem_list.empty())
        return;

    if (render_order_elem_list.size() > gpu_type::k_num_sprites)
    {
        throw std::runtime_error("Too many sprite elems to render.");
    }

    auto& p{ *m_pimpl };

    // Upload sprite information to GPU.
    auto* ui_data_set_data = static_cast<gpu_type::Sprite_data_set_element*>(
        p.g.get_current_frame().sprite_data_set_buffer.get_p_mapped_data());

    for (UI::UI_element const* elem : render_order_elem_list)
    {
        glm_mat4_identity(ui_data_set_data->model_mat);

        uint32_t parent_overflow_count{ 10000 };

        UI::UI_element const* elem_node{ elem };
        while (elem_node != nullptr)
        {
            auto const& etrans{ elem_node->transform};
            mat4 node_model_mat;
            glm_translate_make(node_model_mat, vec3{ etrans.x, etrans.y, 0 });
            glm_rotate_z(node_model_mat, etrans.rot, node_model_mat);
            glm_scale(node_model_mat, vec3{ etrans.w, etrans.h });

            glm_mat4_mul(node_model_mat, ui_data_set_data->model_mat, ui_data_set_data->model_mat);

            elem_node = elem_node->parent;

            parent_overflow_count--;
            if (parent_overflow_count == 0)
                throw std::runtime_error("Circular dependency detected! Youre a slut!");
        }

        glm_vec2_copy(vec2{ elem->transform.anchor.x, elem->transform.anchor.y },
                      ui_data_set_data->anchor);

        ui_data_set_data->opacity = elem->opacity;  // @THOUGHT: inherit opacity from parents??
        ui_data_set_data->texture_idx = elem->texture_idx;

        glm_vec3_copy(const_cast<float_t*>(elem->color_tint), ui_data_set_data->color_tint);

        ui_data_set_data->is_nine_slice = elem->is_nine_slice;
        ui_data_set_data->is_msdf = !elem->render_text.empty();

        ui_data_set_data++;
    }
}

void Shader_sprite::draw(UI_state const& ui_state, float_t camera_view_aspect_ratio)
{
    auto render_order_elem_list{ ui_state.gather_rendering_ui_elements_in_render_order() };
    if (render_order_elem_list.empty())
        return;

    auto& p{ *m_pimpl };

    auto& current_frame{ p.g.get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Setup render.
    uint32_t image_width{ p.g.ui_image.get_extent().width };
    uint32_t image_height{ p.g.ui_image.get_extent().height };

    VkViewport viewport{ .width = static_cast<float_t>(image_width),
                         .height = static_cast<float_t>(image_height),
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ .extent{ .width = image_width, .height = image_height } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.g.all_textures_descriptor_set,
                            0, nullptr);


    Shader_sprite_push_constants push_consts{
        .sprite_data_set_dev_addr = current_frame.sprite_data_set_buffer.get_device_address(),
        .camera_view_aspect_ratio = camera_view_aspect_ratio,
        .render_canvas_height = UI::k_ui_canvas_fixed_height,
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(Shader_sprite_push_constants),
                       &push_consts);

    p.g.combined_static_model.bind(cmd);

    // Render instances.
    uint32_t draw_instance{ 0 };

    for (UI::UI_element* elem : render_order_elem_list)
    {
        p.draw_mesh_for_elem(cmd, *elem, draw_instance);
        draw_instance++;
    }
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
