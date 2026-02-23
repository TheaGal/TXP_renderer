#if TXP_GFX_BACKEND_VULKAN

#include "shader_basic_diffuse.h"

#include "btlogger.h"
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

// struct Shader_basic_diffuse::Impl
struct Shader_basic_diffuse::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , device(g.gfx.device)
        , hdr_draw_image_color(g.hdr_draw_image_color)
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

        size_t texture_entry_i{ 0 };
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
                .maxLod = (float)entry.texture.levelCount,
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
            VkDescriptorImageInfo desc_image_info{
                .sampler = entry.sampler,
                .imageView = entry.image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            entry.gpu_idx = texture_entry_i;
            texture_entry_i++;
        }

        // Descriptor layouts.
        shader_pipeline.descriptor_layout = g.build_descriptor_layout(
            {
                { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
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

    std::string vertex_entry_point_name;
    std::string fragment_entry_point_name;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSet descriptor_set;
        VkDescriptorSetLayout descriptor_layout;
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

void Shader_basic_diffuse::compute(void* param)
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
