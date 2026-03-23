#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_basic_diffuse.h"
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
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_map>


namespace TXP
{
namespace Shader
{
namespace gpu_type
{

/// Material parameters for this shader.
struct Material_param_set
{
    uint32_t texture0_idx;
};

}  // namespace gpu_type

/// Struct for push constants.
struct Shader_basic_diffuse_push_constants  // @TODO: move this to gfx_vulkan_impl!!!
{
    VkDeviceAddress environment_data_dev_addr;
    VkDeviceAddress per_instance_data_collection_dev_addr;
    VkDeviceAddress model_transform_set_dev_addr;
    VkDeviceAddress material_param_set_collection_dev_addr;
};

// struct Shader_basic_diffuse::Impl
struct Shader_basic_diffuse::Impl
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
                                                 .size =
                                                     sizeof(Shader_basic_diffuse_push_constants) };
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
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };

        std::vector<VkFormat> color_attachment_formats{
            g.render_views[0].color_image.get_format(),
        };
        VkPipelineRenderingCreateInfo dynamic_rendering_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
            .pColorAttachmentFormats = color_attachment_formats.data(),
            .depthAttachmentFormat = g.render_views[0].depth_image.get_format(),
        };

        VkPipelineColorBlendAttachmentState blend_attachment{ .colorWriteMask = 0xf, };
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        VkPipelineRasterizationStateCreateInfo rasterization_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
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
        material_param_set_collection_buffer.destroy();
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

    // Parameters for a material.
    std::unordered_map<std::string, gpu_type::Material_param_set> material_name_to_params_map;
    Vk_Buffer::Allocated_buffer material_param_set_collection_buffer;

    // Drawing list.
    std::array<size_t, 2> draw_inst_list_start_end;  // End is exclusive.
};


// class Shader_basic_diffuse
Shader_basic_diffuse::Shader_basic_diffuse(
    Material_organizer& material_organizer,
    Render_model_data_collection& render_model_data_collection,
    void* graphics)
    : m_pimpl(std::make_unique<Impl>(material_organizer,
                                     render_model_data_collection,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_basic_diffuse::~Shader_basic_diffuse() = default;

void Shader_basic_diffuse::make_material(
    std::string const& material_name,
    std::unordered_map<std::string, std::string> const& shader_params)
{
    gpu_type::Material_param_set new_param_set;

    for (auto& [param_key, param_val] : shader_params)
    {
        if (param_key == "texture0")
        {
            new_param_set.texture0_idx = m_pimpl->g.texture_entries.at(param_val).gpu_idx;
        }
        else
            BT_WARNF("Unknown shader param: %s", param_key.c_str());
    }
    if (shader_params.size() != 1)
        throw std::runtime_error("Wrong number of shader params.");

    m_pimpl->material_name_to_params_map.emplace(material_name, std::move(new_param_set));

    m_pimpl->material_organizer.emplace_material(material_name, k_name);
}

void Shader_basic_diffuse::organize_materials()
{   // Create buffer.
    m_pimpl->material_param_set_collection_buffer.create(
        m_pimpl->device,
        m_pimpl->allocator,
        sizeof(gpu_type::Material_param_set) * m_pimpl->material_name_to_params_map.size(),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Upload data.
    char* buffer_addr{ static_cast<char*>(
        m_pimpl->material_param_set_collection_buffer.get_p_mapped_data()) };

    for (auto const& [_, mat_params] : m_pimpl->material_name_to_params_map)
    {
        std::memcpy(buffer_addr, &mat_params, sizeof(mat_params));
        buffer_addr += sizeof(mat_params);
    }
}

void Shader_basic_diffuse::allocate_per_instance_data_slots(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
    size_t& in_out_cur_modmesh_ref_idx)
{
    m_pimpl->draw_inst_list_start_end.front() = in_out_cur_modmesh_ref_idx;

    uint16_t render_obj_idx{ 0 };
    for (auto const& rend_obj : render_object_list)
    {
        // Find number of instances needed for the model.
        auto const& model{ m_pimpl->render_model_data_collection.get_static_model_data_set(
            rend_obj.render_model_idx) };
        size_t num_meshes_in_model{ model.meshes.size() };

        // Collect meshes for this shader.
        auto this_shader_id{ m_pimpl->material_organizer.get_shader_id(k_name) };
        auto const& material_palette{ m_pimpl->material_organizer.get_material_palette(
            rend_obj.material_palette_idx) };
        for (size_t mesh_idx = 0; mesh_idx < num_meshes_in_model; mesh_idx++)
        {
            auto const& material{ material_palette.at(mesh_idx) };
            if (material.shader_id == this_shader_id)
            {   // Uses this shader!
                auto& modmesh_ref_entry{ out_model_mesh_ref_list[in_out_cur_modmesh_ref_idx++] };
                modmesh_ref_entry.render_obj_idx = render_obj_idx;
                modmesh_ref_entry.model_mesh_idx = mesh_idx;
            }
        }

        render_obj_idx++;
    }

    m_pimpl->draw_inst_list_start_end.back() = in_out_cur_modmesh_ref_idx;
}

void Shader_basic_diffuse::draw(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
    void* render_view_param)
{
    if (m_pimpl->draw_inst_list_start_end.back() - m_pimpl->draw_inst_list_start_end.front() == 0)
        return;  // Nothing to draw. Exit early.

    auto& p{ *m_pimpl };

    auto& render_view{ *static_cast<Graphics::Impl::Render_view_data*>(render_view_param) };

    auto& current_frame{ p.g.get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Render.
    VkViewport viewport{ .width = static_cast<float>(render_view.color_image.get_extent().width),
                         .height = static_cast<float>(render_view.color_image.get_extent().height),
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ .extent{ .width = render_view.color_image.get_extent().width,
                               .height = render_view.color_image.get_extent().height } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.g.all_textures_descriptor_set,
                            0, nullptr);

    p.g.combined_static_model.bind(cmd);

    Shader_basic_diffuse_push_constants push_consts{
        .environment_data_dev_addr =
            current_frame.environment_data_buffers[render_view.render_view_idx]
                .get_device_address(),
        .per_instance_data_collection_dev_addr =
            current_frame.per_instance_data_collection_buffer.get_device_address(),
        .model_transform_set_dev_addr =
            current_frame.model_transform_set_buffer.get_device_address(),
        .material_param_set_collection_dev_addr =
            p.material_param_set_collection_buffer.get_device_address(),
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(Shader_basic_diffuse_push_constants),
                       &push_consts);

    // Render instances.
    for (auto draw_instance = m_pimpl->draw_inst_list_start_end.front();
         draw_instance < m_pimpl->draw_inst_list_start_end.back();
         draw_instance++)
    {
        auto const& modmesh_ref{ model_mesh_ref_list[draw_instance] };
        auto const& rend_obj{
            render_object_list[modmesh_ref.render_obj_idx]
        };
        auto const& model{ m_pimpl->render_model_data_collection.get_static_model_data_set(
            rend_obj.render_model_idx) };
        vkCmdDrawIndexed(cmd,
                         model.meshes[modmesh_ref.model_mesh_idx].indices.size(),
                         1,
                         model.first_index_offsets[modmesh_ref.model_mesh_idx],
                         model.vertex_index_offset,
                         draw_instance);
    }
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
