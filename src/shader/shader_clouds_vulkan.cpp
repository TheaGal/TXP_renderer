#include "cglm/vec3.h"
#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_clouds.h"
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
namespace Shader
{

/// Struct for push constants.
struct Shader_clouds_push_constants  // @TODO: move this to gfx_vulkan_impl!!!  @AMEND: what is this message????????
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


// struct Shader_clouds::Impl
struct Shader_clouds::Impl
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
                                                     sizeof(Shader_clouds_push_constants) };
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
        // Create volume texture.
        constexpr VkExtent3D k_cloud_volume_extent{ 256, 32, 256 };
        uint8_t cloud_volume_data[k_cloud_volume_extent.width][k_cloud_volume_extent.height]
                                 [k_cloud_volume_extent.depth];
        for (size_t x = 0; x < k_cloud_volume_extent.width; x++)
            for (size_t y = 0; y < k_cloud_volume_extent.height; y++)
                for (size_t z = 0; z < k_cloud_volume_extent.depth; z++)
                {
                    static vec4s my_sphere{ 128, 16, 128, 16 };
                    cloud_volume_data[x][y][z] =
                        glm_clamp_zo(1.0f - (glm_vec3_distance(vec3{ static_cast<float_t>(x),
                                                                     static_cast<float_t>(y),
                                                                     static_cast<float_t>(z) },
                                                               my_sphere.raw) /
                                             my_sphere.w)) *
                        255;
                }

        #define CLOUD_VOLUME_IMAGE_USE_COMPRESSED_IMAGE_FORMAT 0

        Vk_Image::Allocated_image cloud_volume_image;
        cloud_volume_image.create_image_3d(
        #if CLOUD_VOLUME_IMAGE_USE_COMPRESSED_IMAGE_FORMAT
            VK_FORMAT_BC4_UNORM_BLOCK,
        #else
            VK_FORMAT_R8_UNORM,
        #endif  // CLOUD_VOLUME_IMAGE_USE_COMPRESSED_IMAGE_FORMAT
            k_cloud_volume_extent,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // Create staging buffer.
        assert(cloud_volume_image.get_data_size() == sizeof(cloud_volume_data));
        Vk_Buffer::Allocated_buffer cloud_volume_image_staging;
        cloud_volume_image_staging.create(g.gfx.device,
                                          g.gfx.allocator,
                                          cloud_volume_image.get_data_size(),
                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                              VMA_ALLOCATION_CREATE_MAPPED_BIT);
        memcpy(cloud_volume_image_staging.get_p_mapped_data(),
               cloud_volume_data,
               cloud_volume_image.get_data_size());
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Transfer to GPU
        BT::date_deadline(2026, 11, 15);  // @TODO: put this section into the Allocated_image struct helpers!

        VkFenceCreateInfo fence_one_time_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        VkFence fence_one_time;
        err = vkCreateFence(device, &fence_one_time_info, nullptr, &fence_one_time);
        if (err)
            throw std::runtime_error("Failed to create one-time fence.");

        VkCommandPoolCreateInfo cmd_pool_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = g.gfx.graphics_queue_family_idx,
        };
        VkCommandPool onetime_cmd_pool;
        err = vkCreateCommandPool(g.gfx.device, &cmd_pool_info, nullptr, &onetime_cmd_pool);
        if (err)
            throw std::runtime_error("Vulkan command pool creation failed one-time.");

        Graphics::Impl::Command_buffer one_time_command_buffer;
        one_time_command_buffer.allocate(g.gfx.device, onetime_cmd_pool, true);

        VkCommandBuffer cmd{ one_time_command_buffer.get() };

        if (cloud_volume_image.get_mip_levels() != 1 || cloud_volume_image.get_array_layers() != 1)
            throw std::runtime_error(
                "More than 1 mip or array layer is unsupported currently. You can add support if "
                "you wanna program it in.");

        VkImageMemoryBarrier2 barrier_tex_image{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = cloud_volume_image.get_image().get(),
            .subresourceRange =
                VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };
        VkDependencyInfo barrier_tex_info{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier_tex_image,
        };
        vkCmdPipelineBarrier2(cmd, &barrier_tex_info);
        VkBufferImageCopy copy_region{
            .bufferOffset = 0,
            .imageSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .layerCount = 1,
            },
            .imageExtent = cloud_volume_image.get_extent(),
        };
        vkCmdCopyBufferToImage(cmd,
                               cloud_volume_image_staging.get_buffer(),
                               cloud_volume_image.get_image().get(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &copy_region);
        VkImageMemoryBarrier2 barrier_tex_read{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = cloud_volume_image.get_image().get(),
            .subresourceRange =
                VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };
        barrier_tex_info.pImageMemoryBarriers = &barrier_tex_read;
        vkCmdPipelineBarrier2(cmd, &barrier_tex_info);
        err = vkEndCommandBuffer(cmd);
        if (err)
            throw std::runtime_error("Failed to end one-time command buffer.");

        VkCommandBufferSubmitInfo cbOneTimeSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd
        };
        VkSubmitInfo2 oneTimeSI{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                 .commandBufferInfoCount = 1,
                                 .pCommandBufferInfos = &cbOneTimeSubmitInfo };
        err = vkQueueSubmit2(g.gfx.graphics_queue, 1, &oneTimeSI, fence_one_time);
        if (err)
            throw std::runtime_error("One-time queue submit failed.");

        err = vkWaitForFences(device, 1, &fence_one_time, VK_TRUE, UINT64_MAX);
        if (err)
            throw std::runtime_error("One-time fence wait failed.");

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
        pointer_to_built_config[nullptr] = {};
    }

    ~Impl()
    {
        for (auto&& [_, uploaded_mesh] : pointer_to_uploaded_mesh_map)
        {
            uploaded_mesh.model_buffer->vertex_index_buffer.destroy();
        }

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

        umesh.got_used = true;

        // Delete previously created mesh first if exists.
        if (auto it = pointer_to_uploaded_mesh_map.find(ptr_key);
            it != pointer_to_uploaded_mesh_map.end())
        {
            g.wait_until_gpu_idle();  // this is slow if you could @IMPROVE this??
            it->second.model_buffer->vertex_index_buffer.destroy();
            pointer_to_uploaded_mesh_map.erase(it);
        }

        // Save to uploaded mesh map.
        pointer_to_uploaded_mesh_map.emplace(ptr_key, std::move(umesh));
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
            if (it == pointer_to_uploaded_mesh_map.end() ||
                !pointer_to_built_config.at(elem_as_ptr)
                     .is_equal(elem.render_text, elem.halign, elem.valign))
            {
                generate_text(elem_as_ptr, elem.render_text, elem.halign, elem.valign);
                elem_mesh = &pointer_to_uploaded_mesh_map.at(elem_as_ptr);
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

    struct Built_config
    {
        std::string text;
        UI::UI_element::Align halign;
        UI::UI_element::Align valign;

        bool is_equal(std::string const& param_text,
                      UI::UI_element::Align const param_halign,
                      UI::UI_element::Align const param_valign) const
        {
            return (text == param_text && halign == param_halign && valign == param_valign);
        }
    };
    std::unordered_map<void*, Built_config> pointer_to_built_config;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
    } shader_pipeline;
};


// class Shader_clouds
Shader_clouds::Shader_clouds(Render_model_data_collection& render_model_data_collection,
                             void* graphics)
    : m_pimpl(std::make_unique<Impl>(render_model_data_collection,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_clouds::~Shader_clouds() = default;


namespace
{

bool is_empty_ui_node(UI::UI_element const* elem)
{
    return (elem->texture_idx == UI::k_texture_idx_no_texture && elem->render_text.empty());
}

} // namespace


void Shader_clouds::upload_ui_state_data(UI_state const& ui_state)
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
        if (is_empty_ui_node(elem))
            continue;

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

        bool const has_render_text{ !elem->render_text.empty() };

        ui_data_set_data->opacity = elem->opacity;  // @THOUGHT: inherit opacity from parents??
        ui_data_set_data->texture_idx =
            (has_render_text ? p.g.texture_entries.at("font_msdf_pitagon_serif_regular").gpu_idx
                             : elem->texture_idx);

        glm_vec3_copy(const_cast<float_t*>(elem->color_tint), ui_data_set_data->color_tint);

        ui_data_set_data->is_nine_slice = elem->is_nine_slice;
        ui_data_set_data->is_msdf = has_render_text;

        ui_data_set_data++;
    }
}

void Shader_clouds::compute(UI_state const& ui_state, float_t camera_view_aspect_ratio)
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


    Shader_clouds_push_constants push_consts{
        .sprite_data_set_dev_addr = current_frame.sprite_data_set_buffer.get_device_address(),
        .camera_view_aspect_ratio = camera_view_aspect_ratio,
        .render_canvas_height = UI::k_ui_canvas_fixed_height,
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(Shader_clouds_push_constants),
                       &push_consts);

    p.g.combined_static_model.bind(cmd);

    // Erase unused meshes.
    std::vector<void*> pointers_to_erase;
    for (auto&& [pointer, uploaded_mesh] : p.pointer_to_uploaded_mesh_map)
        if (!uploaded_mesh.got_used)
        {
            uploaded_mesh.model_buffer->vertex_index_buffer.destroy();
            pointers_to_erase.emplace_back(pointer);
            assert(pointer != nullptr);  // you're not supposed to destroy the nullptr one!!!
        }

    for (void* pointer : pointers_to_erase)
        p.pointer_to_uploaded_mesh_map.erase(pointer);

    // Mark all as stale.
    for (auto&& [_, uploaded_mesh] : p.pointer_to_uploaded_mesh_map)
        uploaded_mesh.got_used = false;

    // Render instances.
    uint32_t draw_instance{ 0 };

    for (UI::UI_element* elem : render_order_elem_list)
    {
        if (is_empty_ui_node(elem))
            continue;

        p.draw_mesh_for_elem(cmd, *elem, draw_instance);
        draw_instance++;
    }
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
