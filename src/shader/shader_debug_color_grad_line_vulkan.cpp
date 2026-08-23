#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_debug_color_grad_line.h"
// clang-format on

#include "btglm.h"
#include "btlogger.h"
#include "debug/debug_render_job_internal.h"
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
#include <unordered_map>


namespace TXP
{
namespace Shader
{

/// Struct for push constants.
struct Shader_debug_color_grad_line_push_constants
{
    VkDeviceAddress environment_data_dev_addr;
};

// struct Shader_debug_color_grad_line::Impl
struct Shader_debug_color_grad_line::Impl
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

        // @THEA: @IDEA: @TODO: for creating shaders, often i just need the default shader pipelilne
        //                      settings but one or two tweaks. There should be a func that will
        //                      create the default shader pipeline config, then in the struct I make
        //                      some tweaks, and then submit it for pipeline creation.
        //                        -Thea 2026/08/22

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                 .size =
                                                     sizeof(Shader_debug_color_grad_line_push_constants) };
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            // .setLayoutCount = 1,  // @THEA: this is unneeded.
            // .pSetLayouts = &g.all_textures_descriptor_layout,
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
                                                        .stride = sizeof(debug::Debug_line_vertex),
                                                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, };
        std::vector<VkVertexInputAttributeDescription> vertex_attributes{
            // @THEA: this is different!!
            { .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(debug::Debug_line_vertex, position_x) },
            { .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = offsetof(debug::Debug_line_vertex, color_r) },
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
            .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  // @THEA: this is different.
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
            // .cullMode = VK_CULL_MODE_BACK_BIT,  @THEA: this is different.
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
        debug_lines_vertex_buffer.destroy();
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

    // Vertex buffer for all lines.
    Vk_Buffer::Allocated_buffer debug_lines_vertex_buffer;
    static constexpr size_t vertex_buffer_page_size{ 1024 };
    size_t vertex_buffer_current_size{ vertex_buffer_page_size };
    size_t num_vertexes_to_draw;
};


// class Shader_debug_color_grad_line
Shader_debug_color_grad_line::Shader_debug_color_grad_line(
    Material_organizer& material_organizer,
    Render_model_data_collection& render_model_data_collection,
    void* graphics)
    : m_pimpl(std::make_unique<Impl>(material_organizer,
                                     render_model_data_collection,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_debug_color_grad_line::~Shader_debug_color_grad_line() = default;

void Shader_debug_color_grad_line::make_material(
    std::string const& material_name,
    std::unordered_map<std::string, std::string> const& shader_params)
{
    // Do nothing.
    BT_WARN("This material has no shader params.");

    m_pimpl->material_organizer.emplace_material(material_name, k_name);
}

void Shader_debug_color_grad_line::organize_materials()
{
    // Create buffer.
    m_pimpl->debug_lines_vertex_buffer.create(
        m_pimpl->device,
        m_pimpl->allocator,
        m_pimpl->vertex_buffer_current_size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // @NOTE: upload data happens on-demand.
}

void Shader_debug_color_grad_line::allocate_per_instance_data_slots(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
    size_t& in_out_cur_modmesh_ref_idx)
{
    (void)render_object_list;
    (void)out_model_mesh_ref_list;
    (void)in_out_cur_modmesh_ref_idx;

    // Resize buffer if needed.
    m_pimpl->num_vertexes_to_draw = (debug::calc_num_debug_lines() * 2);
    size_t vertex_size_requirement{ sizeof(debug::Debug_line_vertex) *
                                    m_pimpl->num_vertexes_to_draw };

    bool vertex_buffer_resize_needed{ false };
    while (vertex_size_requirement > m_pimpl->vertex_buffer_current_size)
    {
        m_pimpl->vertex_buffer_current_size += Impl::vertex_buffer_page_size;
        vertex_buffer_resize_needed = true;
    }

    if (vertex_buffer_resize_needed)
    {
        m_pimpl->debug_lines_vertex_buffer.destroy();
        m_pimpl->debug_lines_vertex_buffer.create(
            m_pimpl->device,
            m_pimpl->allocator,
            m_pimpl->vertex_buffer_current_size,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Write to vertex buffer.
    debug::write_debug_line_mem(m_pimpl->debug_lines_vertex_buffer.get_p_mapped_data());
}

void Shader_debug_color_grad_line::draw(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
    void* render_view_param)
{
    (void)render_object_list;
    (void)model_mesh_ref_list;

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
    // @THEA: this is unneeded.
    // vkCmdBindDescriptorSets(cmd,
    //                         VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                         p.shader_pipeline.pipeline_layout,
    //                         0,
    //                         1, &p.g.all_textures_descriptor_set,
    //                         0, nullptr);


    Shader_debug_color_grad_line_push_constants push_consts{
        .environment_data_dev_addr =
            current_frame.environment_data_buffers[render_view.render_view_idx]
                .get_device_address(),
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(Shader_debug_color_grad_line_push_constants),
                       &push_consts);

    // Render lines.
    VkDeviceSize offset{ 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &p.debug_lines_vertex_buffer.get_buffer(), &offset);

    vkCmdDraw(cmd, p.num_vertexes_to_draw, 1, 0, 0);
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
