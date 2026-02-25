#if TXP_GFX_BACKEND_VULKAN

#include "shader_gradient.h"

#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <memory>
#include <stdexcept>


namespace TXP
{
namespace Shader
{

// struct Shader_gradient::Impl
struct Shader_gradient::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , device(g.gfx.device)
        , hdr_draw_image_color(g.hdr_draw_image_color)
    {
    #define WRAP_INTO_OWN_FUNC 1
    #if WRAP_INTO_OWN_FUNC
        auto refl_data = Shader_Creation::read_slang_reflection(k_name);

        if (refl_data.entryPoints.size() != 1 ||
            refl_data.entryPoints.front().stage != "compute" ||
            refl_data.entryPoints.front().threadGroupSize.size() != 3 ||
            refl_data.entryPoints.front().threadGroupSize[0] <= 0 ||
            refl_data.entryPoints.front().threadGroupSize[1] <= 0 ||
            refl_data.entryPoints.front().threadGroupSize[2] <= 0)
            std::runtime_error("Malformed shader data.");

        compute_entry_point_name = refl_data.entryPoints.front().name;

        thread_grp_sizes.width  = refl_data.entryPoints.front().threadGroupSize[0];
        thread_grp_sizes.height = refl_data.entryPoints.front().threadGroupSize[1];
        thread_grp_sizes.depth  = refl_data.entryPoints.front().threadGroupSize[2];
    #endif // WRAP_INTO_OWN_FUNC


        // @TODO: for vv below vv pull out the `build_Descriptor_layout()` and
        //        `load_shader_module()` functions.

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Descriptors.

        // Descriptor layouts.
        shader_pipeline.descriptor_layout = g.build_descriptor_layout(
            {
                { 0, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE } },
            },
            VK_SHADER_STAGE_COMPUTE_BIT,
            0);

        // Descriptors.
        shader_pipeline.descriptor_set =
            g.global_descriptor_allocator.allocate(shader_pipeline.descriptor_layout);

        VkDescriptorImageInfo img_info{
            .imageView = hdr_draw_image_color.get_image_view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet img_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = shader_pipeline.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &img_info,
        };

        vkUpdateDescriptorSets(device, 1, &img_write, 0, nullptr);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &shader_pipeline.descriptor_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        err = vkCreatePipelineLayout(device,
                                     &pipeline_layout_info,
                                     nullptr,
                                     &shader_pipeline.pipeline_layout);
        if (err)
            throw std::runtime_error("Failed to create pipeline layout.");

        // Create pipeline.
        VkShaderModule shader_module{ g.load_shader_module(
            Shader_Creation::get_shader_module_path(k_name)) };

        VkPipelineShaderStageCreateInfo stage_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = compute_entry_point_name.c_str(),
            // .pSpecializationInfo = nullptr,  // @RESEARCH: research this if you want!
        };

        VkComputePipelineCreateInfo compute_pipeline_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .stage = stage_info,
            .layout = shader_pipeline.pipeline_layout,
        };

        err = vkCreateComputePipelines(device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &compute_pipeline_info,
                                       nullptr,
                                       &shader_pipeline.pipeline);
        if (err)
            throw std::runtime_error("Failed to create compute pipeline.");

        // Cleanup.
        vkDestroyShaderModule(device, shader_module, nullptr);
    }


    TXP::Graphics::Impl& g;
    VkDevice device;
    Vk_Image::Allocated_image& hdr_draw_image_color;

    std::string compute_entry_point_name;
    VkExtent3D thread_grp_sizes;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSet descriptor_set;
        VkDescriptorSetLayout descriptor_layout;
    } shader_pipeline;
};


// class Shader_gradient
Shader_gradient::Shader_gradient(void* graphics)
    : m_pimpl(std::make_unique<Impl>(*static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_gradient::~Shader_gradient() = default;

void Shader_gradient::compute(void* param)
{
    auto& p{ *m_pimpl };

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(
        cmd,
        { { &p.hdr_draw_image_color.get_image(), VK_IMAGE_LAYOUT_GENERAL } });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.shader_pipeline.descriptor_set,
                            0, nullptr);

    // @TODO: move this into a real function.
    static auto const k_cmd_dispatch_fn =
        [](VkCommandBuffer cmd, VkExtent3D dispatch_thread_sizes, VkExtent3D thread_group_sizes) {
            vkCmdDispatch(cmd,
                          (dispatch_thread_sizes.width + thread_group_sizes.width - 1) /
                              thread_group_sizes.width,
                          (dispatch_thread_sizes.height + thread_group_sizes.height - 1) /
                              thread_group_sizes.height,
                          (dispatch_thread_sizes.depth + thread_group_sizes.depth - 1) /
                              thread_group_sizes.depth);
        };

    k_cmd_dispatch_fn(cmd, p.hdr_draw_image_color.get_extent(), p.thread_grp_sizes);
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
