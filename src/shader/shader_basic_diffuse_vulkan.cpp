#if TXP_GFX_BACKEND_VULKAN

#include "shader_basic_diffuse.h"

#include "btglm.h"
#include "btlogger.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan/vk_structs.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <cstddef>
#include <memory>
#include <stdexcept>


namespace TXP
{
namespace Shader
{

/// Struct for Environment_data.
struct GPU_environment_data
{
    mat4 projection;
    mat4 view;
    vec4 light_pos;
    uint32_t basic_lighting;
};

/// Struct for Model_transform_set.
struct GPU_model_transform_set
{
    mat4 transforms[65535];
};

/// Struct for push constants.
struct Shader_basic_diffuse_push_constants  // @TODO: move this to gfx_vulkan_impl!!!
{
    VkDeviceAddress environment_data_dev_addr;
    VkDeviceAddress model_transform_set_dev_addr;
};

// struct Shader_basic_diffuse::Impl
struct Shader_basic_diffuse::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , device(g.gfx.device)
        , hdr_draw_image_color(g.hdr_draw_image_color)
        , hdr_draw_image_depth(g.hdr_draw_image_depth)
    {
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


        // @TODO: for vv below vv pull out the `build_Descriptor_layout()` and
        //        `load_shader_module()` functions.

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Buffers.
        for (auto& frame : g.frames)
        {
            frame.environment_data_buffer.create(
                g.gfx.device,
                g.gfx.allocator,
                sizeof(GPU_environment_data),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);

            frame.model_transform_set_buffer.create(
                g.gfx.device,
                g.gfx.allocator,
                sizeof(GPU_model_transform_set),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Descriptors.

        // Create all-texture descriptor image infos.
        all_texture_infos.reserve(g.texture_entries.size());

        if (g.texture_entries.size() > 16)
        {
            BT_ERRORF(
                "Texture entries exceeded macOS limit of maxPerStageDescritorSamplers: %zu. At "
                "this point, separate out the COMBINED_IMAGE_SAMPLERS thingies into sampled images "
                "and samplers. Change this error message to just error when the number of samplers "
                "goes over whatever the device limit is, or 16, whichever is smaller.",
                g.texture_entries.size());
            assert(false);
            throw std::runtime_error("Too many samplers.");
        }
        if (g.texture_entries.size() > 256)
        {
            BT_ERRORF(
                "Texture entries exceeded macOS limit of maxPerStageDescritorSampledImages: %zu. "
                "At this point, you need to make a material-sampledimage-sampler batcher that can "
                "handle the sampler and sampledimage device limits. Once you make this, uncap the "
                "sampler limit from the random 16 limit. Also, change this error message to error "
                "if the sampled image size exceeds device limits once you implement the batcher "
                "system. Good luck, future Thea!!",
                g.texture_entries.size());
            assert(false);
            throw std::runtime_error("Too many sampled images.");
        }

        std::vector<VkDescriptorImageInfo> desc_img_infos;
        desc_img_infos.reserve(g.texture_entries.size());

        for (auto& [name, entry] : g.texture_entries)
        {
            VkResult err;

            // Create texture sampler.
            VkSamplerCreateInfo sampler_info{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = (entry.texture.levelCount == 1 ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                             : VK_SAMPLER_MIPMAP_MODE_LINEAR),
                .anisotropyEnable = VK_TRUE,  // @TODO: put this into a setting!
                .maxAnisotropy = 8.0f,  // 8 is a widely supported value for max anisotropy.  @TODO: put this into a setting!
                .maxLod = static_cast<float>(entry.texture.levelCount),
            };

            err = vkCreateSampler(g.gfx.device, &sampler_info, nullptr, &entry.sampler);
            if (err)
                throw std::runtime_error("Failed to create VK sampler.");

            // Create texture image view.
            VkImageViewCreateInfo image_view_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .image = entry.texture.image,
                .viewType = entry.texture.viewType,
                .format = entry.texture.imageFormat,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = entry.texture.levelCount,
                    .baseArrayLayer = 0,
                    .layerCount = entry.texture.layerCount,
                },
            };

            err = vkCreateImageView(g.gfx.device, &image_view_info, nullptr, &entry.image_view);
            if (err)
                throw std::runtime_error("Failed to create VK image view.");

            // Create descriptor.
            VkDescriptorImageInfo desc_img_info{
                .sampler = entry.sampler,
                .imageView = entry.image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            entry.gpu_idx = desc_img_infos.size();
            desc_img_infos.emplace_back(std::move(desc_img_info));
        }

        // Descriptor layouts.
        using Descriptor_type_info = Graphics::Impl::Descriptor_type_info;

        shader_pipeline.textures_descriptor_layout = g.build_descriptor_layout(
            {
                { 0,
                  Descriptor_type_info{ .descriptor_type =
                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .use_variable_descriptor_count_binding_flag = true,
                                        .variable_descriptor_count =
                                            static_cast<uint32_t>(g.texture_entries.size()) } },
            },
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0);

        // Descriptors.
        shader_pipeline.textures_descriptor_set =
            g.global_descriptor_allocator.allocate(shader_pipeline.textures_descriptor_layout,
                                                   static_cast<uint32_t>(g.texture_entries.size()));

        VkWriteDescriptorSet imgs_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .dstSet = shader_pipeline.textures_descriptor_set,
                                         .dstBinding = 0,
                                         .descriptorCount =
                                             static_cast<uint32_t>(g.texture_entries.size()),
                                         .descriptorType =
                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         .pImageInfo = desc_img_infos.data() };
        vkUpdateDescriptorSets(device, 1, &imgs_write, 0, nullptr);

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
            .pSetLayouts = &shader_pipeline.textures_descriptor_layout,
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
            hdr_draw_image_color.get_format(),
        };
        VkPipelineRenderingCreateInfo dynamic_rendering_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
            .pColorAttachmentFormats = color_attachment_formats.data(),
            .depthAttachmentFormat = hdr_draw_image_depth.get_format(),
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


    TXP::Graphics::Impl& g;
    VkDevice device;
    Vk_Image::Allocated_image& hdr_draw_image_color;
    Vk_Image::Allocated_image& hdr_draw_image_depth;

    std::string vertex_entry_point_name;
    std::string fragment_entry_point_name;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSet textures_descriptor_set;
        VkDescriptorSetLayout textures_descriptor_layout;
    } shader_pipeline;

    // @THEA: @NOCHECKIN: the vv below vv needs to get promoted to be created at the same time as the ktxtextures get loaded!!!!
#define WRAP_INTO_OWN_FUNC 1
#if WRAP_INTO_OWN_FUNC
    /// Descriptor set for `textures`.
    std::vector<VkDescriptorImageInfo> all_texture_infos;
#endif // WRAP_INTO_OWN_FUNC
};


// class Shader_basic_diffuse
Shader_basic_diffuse::Shader_basic_diffuse(void* graphics)
    : m_pimpl(std::make_unique<Impl>(*static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_basic_diffuse::~Shader_basic_diffuse() = default;

void Shader_basic_diffuse::draw(void* param)
{
    auto& p{ *m_pimpl };

    auto& current_frame{ p.g.get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Ready images.
    Vk_Image::Image::transition_to(
        cmd,
        { { &p.hdr_draw_image_color.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },
          { &p.hdr_draw_image_depth.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL } });

#define WRAP_INTO_OWN_FUNC 1
#if WRAP_INTO_OWN_FUNC
    // Begin rendering.
    VkRenderingAttachmentInfo color_attachment =
        Vk_Structs::txp_vk_attachment_info(p.hdr_draw_image_color.get_image_view(),
                                           nullptr,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depth_attachment =
        Vk_Structs::txp_vk_attachment_info(p.hdr_draw_image_depth.get_image_view(),
                                           nullptr,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = Vk_Structs::txp_vk_render_info(
        VkExtent2D{ .width = p.hdr_draw_image_color.get_extent().width,
                    .height = p.hdr_draw_image_color.get_extent().height },
        &color_attachment,
        &depth_attachment);
    vkCmdBeginRendering(cmd, &render_info);
#endif // WRAP_INTO_OWN_FUNC

    // Render.
    VkViewport viewport{ .width = static_cast<float>(p.hdr_draw_image_color.get_extent().width),
                         .height = static_cast<float>(p.hdr_draw_image_color.get_extent().height),
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ .extent{ .width = p.hdr_draw_image_color.get_extent().width,
                               .height = p.hdr_draw_image_color.get_extent().height } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.shader_pipeline.textures_descriptor_set,
                            0, nullptr);

    p.g.combined_static_model.bind(cmd);

    Shader_basic_diffuse_push_constants push_consts{
        .environment_data_dev_addr = current_frame.environment_data_buffer.get_device_address(),
        .model_transform_set_dev_addr =
            current_frame.model_transform_set_buffer.get_device_address(),
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
