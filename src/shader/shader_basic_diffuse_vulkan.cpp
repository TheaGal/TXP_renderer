#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_basic_diffuse.h"
// clang-format on

#include "btglm.h"
#include "btlogger.h"
#include "material_collection/material_collection.h"
#include "renderer/gfx.h"
#include "renderer/gfx_vulkan/vk_buffer.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan/vk_structs.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

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
    Impl(TXP::Material_collection& mat_coll, TXP::Graphics::Impl& graphics)
        : material_collection(mat_coll)
        , g(graphics)
        , device(g.gfx.device)
        , allocator(g.gfx.allocator)
    {
        material_collection.emplace_shader(k_name);

    #define WRAP_INTO_OWN_FUNC 1
    #if WRAP_INTO_OWN_FUNC
        auto refl_data = Shader_Creation::read_slang_reflection(k_name);

        if (refl_data.entryPoints.size() != 2 ||
            refl_data.entryPoints[0].stage != "vertex" ||
            refl_data.entryPoints[0].stage != "fragment")
            std::runtime_error("Malformed shader data.");

        vertex_entry_point_name = refl_data.entryPoints[0].name;
        fragment_entry_point_name = refl_data.entryPoints[1].name;
    #endif // WRAP_INTO_OWN_FUNC

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


    TXP::Material_collection& material_collection;

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

    // @THEA: @NOCHECKIN: the vv below vv needs to get promoted to be created at the same time as the ktxtextures get loaded!!!!
#define WRAP_INTO_OWN_FUNC 1
#if WRAP_INTO_OWN_FUNC
    /// Descriptor set for `textures`.
    std::vector<VkDescriptorImageInfo> all_texture_infos;
#endif // WRAP_INTO_OWN_FUNC
};


// class Shader_basic_diffuse
Shader_basic_diffuse::Shader_basic_diffuse(Material_collection& material_collection, void* graphics)
    : m_pimpl(
          std::make_unique<Impl>(material_collection, *static_cast<TXP::Graphics::Impl*>(graphics)))
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

    m_pimpl->material_collection.emplace_material(material_name, k_name);
}

void Shader_basic_diffuse::build_material_collection()
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

void Shader_basic_diffuse::draw(void* render_view_param)
{
    auto& p{ *m_pimpl };

    auto& render_view{ *static_cast<Graphics::Impl::Render_view_data*>(render_view_param) };

    auto& current_frame{ p.g.get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Ready images.
    Vk_Image::Image::transition_to(
        cmd,
        { { &render_view.color_image.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },
          { &render_view.depth_image.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL } });

#define WRAP_INTO_OWN_FUNC 1
#if WRAP_INTO_OWN_FUNC
    // Begin rendering.
    VkClearValue color_clear_value{
        .color{ .float32{ 0, 0, 0, 1 } },
    };
    VkRenderingAttachmentInfo color_attachment =
        Vk_Structs::txp_vk_attachment_info(render_view.color_image.get_image_view(),
                                           &color_clear_value,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkClearValue depth_clear_value{
        .depthStencil{ .depth = 1.0f, .stencil = 0 },
    };
    VkRenderingAttachmentInfo depth_attachment =
        Vk_Structs::txp_vk_attachment_info(render_view.depth_image.get_image_view(),
                                           &depth_clear_value,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = Vk_Structs::txp_vk_render_info(
        VkExtent2D{ .width = render_view.color_image.get_extent().width,
                    .height = render_view.color_image.get_extent().height },
        &color_attachment,
        &depth_attachment);
    vkCmdBeginRendering(cmd, &render_info);
#endif // WRAP_INTO_OWN_FUNC

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

    // @TODO: @THEA: this needs to be some kind of draw function for certain meshes that want to be drawn by this shader.
    static auto const k_cmd_draw_fn =
        [](VkCommandBuffer cmd) {
            vkCmdDrawIndexed(cmd, 4224, 1, 0, 0, 0);  // @HACK: first uploaded model has 4224 idxs
        };

    k_cmd_draw_fn(cmd);

#define WRAP_INTO_OWN_FUNC 1
#if WRAP_INTO_OWN_FUNC
    // End rendering.
    vkCmdEndRendering(cmd);
#endif // WRAP_INTO_OWN_FUNC
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
