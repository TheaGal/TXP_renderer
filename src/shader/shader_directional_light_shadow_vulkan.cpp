#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_directional_light_shadow.h"
// clang-format on

#include "btglm.h"
#include "btlogger.h"
#include "material_organizer/material_organizer.h"
#include "renderer/gfx.h"
#include "renderer/gfx_vulkan/vk_buffer.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan/vk_structs.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader/shader_support.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <tracy/Tracy.hpp>
#include <unordered_map>


namespace TXP
{
namespace Shader
{

/// Struct for push constants.
struct Shader_directional_light_shadow_push_constants  // @TODO: move this to gfx_vulkan_impl!!!
{
    mat4 light_projection;
    mat4 light_view;
    VkDeviceAddress per_instance_data_collection_dev_addr;
    VkDeviceAddress model_transform_set_dev_addr;
};
static_assert(offsetof(Shader_directional_light_shadow_push_constants, light_projection) % k_mat4_alignment == 0);
static_assert(offsetof(Shader_directional_light_shadow_push_constants, light_view) % k_mat4_alignment == 0);

// struct Shader_directional_light_shadow::Impl
struct Shader_directional_light_shadow::Impl
{
    Impl(TXP::Material_organizer& mat_coll,
         TXP::Render_model_data_collection& rend_mod_data_coll,
         TXP::Graphics::Impl& graphics)
        : material_organizer(mat_coll)
        , render_model_data_collection(rend_mod_data_coll)
        , g(graphics)
        , device(g.gfx.device)
        , allocator(g.gfx.allocator)
    {
        material_organizer.emplace_shader(k_name);

        Shader_Support::fetch_graphics_shader_info(k_name,
                                                   vertex_entry_point_name,
                                                   fragment_entry_point_name);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                 .size = sizeof(Shader_directional_light_shadow_push_constants) };
        VkDescriptorSetLayout desc_layouts[]{ g.shadow_atlas_texture_descriptor_layout,
                                              g.all_textures_descriptor_layout };
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 2,
            .pSetLayouts = desc_layouts,
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
                                                        .stride = sizeof(Vertex),
                                                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, };
        std::vector<VkVertexInputAttributeDescription> vertex_attributes{
            { .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, position_x) },
            { .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, normal_x) },
            { .location = 2,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Vertex, uv_x) },
            { .location = 3,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = offsetof(Vertex, tangent_x) },
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,  // Reverse depth.
        };

        std::vector<VkFormat> color_attachment_formats{
            g.shadow_atlas_texture_image.get_format(),
        };
        VkPipelineRenderingCreateInfo dynamic_rendering_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
            .pColorAttachmentFormats = color_attachment_formats.data(),
            .depthAttachmentFormat = g.shadow_atlas_depth_image.get_format(),
        };

        VkPipelineColorBlendAttachmentState blend_attachment{ .colorWriteMask = 0xf, };
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        VkPipelineRasterizationStateCreateInfo rasterization_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_TRUE,  // @THEA: this is different!!!
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
            .pDepthStencilState = &depth_stencil_state,
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


    TXP::Material_organizer& material_organizer;
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

    // Drawing list.
    std::array<size_t, 2> draw_inst_list_start_end;  // End is exclusive.
};


// class Shader_directional_light_shadow
Shader_directional_light_shadow::Shader_directional_light_shadow(
    Material_organizer& material_organizer,
    Render_model_data_collection& render_model_data_collection,
    void* graphics)
    : m_pimpl(std::make_unique<Impl>(material_organizer,
                                     render_model_data_collection,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_directional_light_shadow::~Shader_directional_light_shadow() = default;

void Shader_directional_light_shadow::allocate_per_instance_data_slots(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
    size_t& in_out_cur_modmesh_ref_idx)
{
    m_pimpl->draw_inst_list_start_end.front() = in_out_cur_modmesh_ref_idx;

    uint16_t render_obj_idx{ 0 };
    for (auto const& rend_obj : render_object_list)
    {
        auto rend_model_idx{
            m_pimpl->render_model_data_collection.translate_to_static_model_data_set_idx(
                rend_obj.render_model_idx)
        };

        // Find number of instances needed for the model.
        auto const& model{ m_pimpl->render_model_data_collection.get_static_model_data_set(
            rend_model_idx) };
        size_t num_meshes_in_model{ model.meshes.size() };

        // Collect meshes for this shader.
        bool use_all_meshes{ rend_obj.sub_mesh_idx == (uint16_t)-1 };

        // auto this_shader_id{ m_pimpl->material_organizer.get_shader_id(k_name) };
        // auto const& material_palette{ m_pimpl->material_organizer.get_material_palette(
        //     rend_obj.material_palette_idx) };

        for (size_t mesh_idx = (use_all_meshes ? 0 : rend_obj.sub_mesh_idx);
             mesh_idx < num_meshes_in_model;
             mesh_idx++)
        {
            // auto const& material{ material_palette.at(mesh_idx) };
            // if (material.shader_id == this_shader_id)
            {   // Uses this shader!
                auto& modmesh_ref_entry{ out_model_mesh_ref_list[in_out_cur_modmesh_ref_idx++] };
                modmesh_ref_entry.render_obj_idx = render_obj_idx;
                modmesh_ref_entry.model_mesh_idx = mesh_idx;
            }

            if (!use_all_meshes)
                break;
        }

        render_obj_idx++;
    }

    m_pimpl->draw_inst_list_start_end.back() = in_out_cur_modmesh_ref_idx;
}

size_t Shader_directional_light_shadow::get_num_shadow_atlas_entries_to_draw() const
{
    return m_pimpl->g.shadow_atlas_entries.size();
}

void Shader_directional_light_shadow::draw(
    size_t shadow_atlas_entry_idx,
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
    Render_layer const render_layer_mask)
{
    ZoneScoped;

    auto& p{ *m_pimpl };
    if (p.draw_inst_list_start_end.back() - p.draw_inst_list_start_end.front() == 0)
        return;  // Nothing to draw. Exit early.

    auto& current_frame{ p.g.get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    TracyVkZone(current_frame.graphics_queue_command_buffer.get_tracy(),
                cmd,
                "Draw Shader_directional_light_shadow");

    // Get shadow atlas entry.
    auto const& shadow_atlas_entry{ p.g.shadow_atlas_entries[shadow_atlas_entry_idx] };

    // Render.
    VkViewport viewport{ .x = static_cast<float_t>(shadow_atlas_entry.atlas_pos[0]),
                         .y = static_cast<float_t>(shadow_atlas_entry.atlas_pos[1]),
                         .width = static_cast<float_t>(shadow_atlas_entry.atlas_extent[0]),
                         .height = static_cast<float_t>(shadow_atlas_entry.atlas_extent[1]),
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ .offset{ shadow_atlas_entry.atlas_pos[0], shadow_atlas_entry.atlas_pos[1] },
                      .extent{ .width = static_cast<uint32_t>(shadow_atlas_entry.atlas_extent[0]),
                               .height = static_cast<uint32_t>(shadow_atlas_entry.atlas_extent[1]) } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.shader_pipeline.pipeline);


    Shader_directional_light_shadow_push_constants push_consts{
        .per_instance_data_collection_dev_addr =
            current_frame.per_instance_data_collection_buffer.get_device_address(),
        .model_transform_set_dev_addr =
            current_frame.model_transform_set_buffer.get_device_address(),
    };
    glm_mat4_copy(const_cast<vec4*>(shadow_atlas_entry.light_projection),
                  push_consts.light_projection);
    glm_mat4_copy(const_cast<vec4*>(shadow_atlas_entry.light_view), push_consts.light_view);

    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(Shader_directional_light_shadow_push_constants),
                       &push_consts);

    // Combined model binding state.
    int32_t bound_combined_model{ -1 };  // -1:unbound  0:static-model  1:deformed-model
    int32_t num_combined_model_binding_changes{ 0 };

    // Render instances.
    for (bool const using_static_model : { true, false })
    {
        ZoneScopedN("using 1st static 2nd deformed model");

        auto& using_model{ using_static_model ? p.g.combined_static_model
                                              : p.g.combined_deformed_model };
        bool bound_using_model{ false };

        for (auto draw_instance = p.draw_inst_list_start_end.front();
             draw_instance < p.draw_inst_list_start_end.back();
             draw_instance++)
        {
            ZoneScopedN("draw instance");

            auto const& modmesh_ref{ model_mesh_ref_list[draw_instance] };
            auto const& rend_obj{ render_object_list[modmesh_ref.render_obj_idx] };

            if (!rend_obj.is_visible || (rend_obj.layer & render_layer_mask) == 0)
                continue;

            bool is_static_model{ p.render_model_data_collection.is_static_model_idx(
                rend_obj.render_model_idx) };
            
            if (is_static_model != using_static_model)
                continue;  // Skip this draw instance since wrong model bound rn.

            // Bind combined model if not done so already.
            if (!bound_using_model)
            {
                using_model.bind(cmd);
                bound_using_model = true;
            }

            // Draw single model.
            auto const& model{ is_static_model
                                   ? p.render_model_data_collection.get_static_model_data_set(
                                         rend_obj.render_model_idx)
                                   : p.render_model_data_collection
                                         .get_deformed_model_data_set(rend_obj.render_model_idx)
                                         .deformed_model };
            vkCmdDrawIndexed(cmd,
                             model.meshes[modmesh_ref.model_mesh_idx].indices.size(),
                             1,
                             model.first_index_offsets[modmesh_ref.model_mesh_idx],
                             model.vertex_index_offset,
                             draw_instance);
        }
    }
}

void Shader_directional_light_shadow::wait_until_completion()
{
    ZoneScoped;

    auto& p{ *m_pimpl };

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(cmd,
                                   { { &p.g.shadow_atlas_texture_image.get_image(),
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } });
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
