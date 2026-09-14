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
#include <memory>
#include <stdexcept>


namespace TXP
{
namespace Shader
{

/// Struct for push constants.
struct Shader_sprite_push_constants  // @TODO: move this to gfx_vulkan_impl!!!  @AMEND: what is this message????????
{
    VkDeviceAddress sprite_data_set_dev_addr;
    float_t camera_view_aspect_ratio;
    float_t render_canvas_height;
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
        VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
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

        // @THEA: satisfy this!!!
        //        figure out the color blend.
        BT::date_deadline(2026, 9, 15);
        VkPipelineColorBlendAttachmentState blend_attachment{ .colorWriteMask = 0xf, };
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
    }

    ~Impl()
    {
        vkDestroyPipelineLayout(device, shader_pipeline.pipeline_layout, nullptr);
        vkDestroyPipeline(device, shader_pipeline.pipeline, nullptr);
    }


    TXP::Render_model_data_collection& render_model_data_collection;

    TXP::Graphics::Impl& g;
    VkDevice device;
    VmaAllocator allocator;

    std::string vertex_entry_point_name;
    std::string fragment_entry_point_name;

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
        }

        glm_vec2_copy(vec2{ elem->transform.anchor.x, elem->transform.anchor.y },
                      ui_data_set_data->anchor);

        ui_data_set_data->opacity = elem->opacity;  // @THOUGHT: inherit opacity from parents??
        ui_data_set_data->texture_idx = elem->texture_idx;
        ui_data_set_data->is_nine_slice = elem->is_nine_slice;

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
        .render_canvas_height = 360,  // @HARDCODE: typical 640x360 size.
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
        // Depend on in-shader vertex data to construct a mesh on the fly.
        vkCmdDraw(cmd,
                  elem->is_nine_slice ? 6 * 9 : 6,
                  1,
                  0,
                  draw_instance);

        draw_instance++;
    }
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
