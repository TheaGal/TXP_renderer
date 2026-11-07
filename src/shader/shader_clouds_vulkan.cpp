#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_clouds.h"
// clang-format on

#include "btdatecheck.h"
#include "btglm.h"
#include "camera/camera_internal.h"
#include "renderer/gfx.h"
#include "renderer/gfx_vulkan/vk_buffer.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader/shader_support.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>


namespace TXP
{
namespace Shader
{

/// Struct for push constants.
struct Shader_clouds_push_constants  // @TODO: move this to gfx_vulkan_impl!!!  @AMEND: what is this message????????
{
    mat4 inv_cam_projview;
    vec3 voxel_grid_origin;
    float_t voxel_size;
};


// struct Shader_clouds::Impl
struct Shader_clouds::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , device(g.gfx.device)
        , allocator(g.gfx.allocator)
    {
        Shader_Support::fetch_compute_shader_info(k_name,
                                                  compute_entry_point_name,
                                                  thread_grp_sizes.width,
                                                  thread_grp_sizes.height,
                                                  thread_grp_sizes.depth);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Descriptors.

        // Descriptor layouts.
        shader_pipeline.descriptor_layout = g.build_descriptor_layout(
            {
                { 0, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE } },
                { 1, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE } },
                { 2, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE } },
            },
            VK_SHADER_STAGE_COMPUTE_BIT,
            0);

        // Defer building descriptors.
        shader_pipeline.is_descriptor_set_valid = false;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                 .size = sizeof(Shader_clouds_push_constants) };
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &shader_pipeline.descriptor_layout,
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

        cloud_volume_image = Vk_Image::Allocated_image::create_image_3d(
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
        one_time_command_buffer.reset();

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

        one_time_command_buffer.finish();

        VkCommandBufferSubmitInfo one_time_command_buffer_submit_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd,
        };
        VkSubmitInfo2 one_time_submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &one_time_command_buffer_submit_info,
        };
        err = vkQueueSubmit2(g.gfx.graphics_queue, 1, &one_time_submit_info, fence_one_time);
        if (err)
            throw std::runtime_error("One-time queue submit failed.");

        err = vkWaitForFences(device, 1, &fence_one_time, VK_TRUE, UINT64_MAX);
        if (err)
            throw std::runtime_error("One-time fence wait failed.");

        // Cleanup.
        cloud_volume_image_staging.destroy();
        vkDestroyFence(device, fence_one_time, nullptr);
        vkDestroyCommandPool(device, onetime_cmd_pool, nullptr);
    }

    ~Impl()
    {
        cloud_volume_image.teardown();
        vkDestroyPipelineLayout(device, shader_pipeline.pipeline_layout, nullptr);
        vkDestroyPipeline(device, shader_pipeline.pipeline, nullptr);
    }

    void build_descriptor()
    {
        g.wait_until_gpu_idle();

        auto& sp{ shader_pipeline };

        // Reallocate.
        sp.descriptor_set_images.clear();
        sp.descriptor_set_images.reserve(3);

        // Build descriptors.
        size_t const render_view_idx{ 0 };
        auto& render_view{ g.render_views[render_view_idx] };

        // Insert image refs.
        sp.descriptor_set_images.emplace_back(&cloud_volume_image.get_image());
        sp.descriptor_set_images.emplace_back(&render_view.depth_image.get_image());
        sp.descriptor_set_images.emplace_back(&g.clouds_image.get_image());

        sp.destination_image = &g.clouds_image.get_image();

        // Allocate needed descriptors.
        if (sp.descriptor_set == VK_NULL_HANDLE)
        {
            sp.descriptor_set = g.global_descriptor_allocator.allocate(sp.descriptor_layout);
        }

        // Write descriptors.
        std::vector<std::pair<VkImageView, VkDescriptorType>> const image_views_w_desc_type{
            { cloud_volume_image.get_image_view(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            { render_view.depth_image.get_image_view(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            { g.clouds_image.get_image_view(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        };

        std::vector<VkDescriptorImageInfo> desc_image_infos;
        desc_image_infos.reserve(image_views_w_desc_type.size());

        std::vector<VkWriteDescriptorSet> desc_image_writes;
        desc_image_writes.reserve(image_views_w_desc_type.size());

        for (uint32_t i = 0; i < image_views_w_desc_type.size(); i++)
        {
            desc_image_infos.emplace_back(VkDescriptorImageInfo{
                .imageView = image_views_w_desc_type[i].first,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
            desc_image_writes.emplace_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = sp.descriptor_set,
                .dstBinding = i,
                .descriptorCount = 1,
                .descriptorType = image_views_w_desc_type[i].second,
                .pImageInfo = &desc_image_infos[i],
            });
        }

        vkUpdateDescriptorSets(device,
                               desc_image_writes.size(),
                               desc_image_writes.data(),
                               0,
                               nullptr);

        // Finished.
        sp.is_descriptor_set_valid = true;
    }


    TXP::Graphics::Impl& g;
    VkDevice device;
    VmaAllocator allocator;

    std::string compute_entry_point_name;
    VkExtent3D thread_grp_sizes;

    Vk_Image::Allocated_image cloud_volume_image;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };
        VkDescriptorSetLayout descriptor_layout;

        std::vector<Vk_Image::Image*> descriptor_set_images;
        Vk_Image::Image* destination_image;

        bool is_descriptor_set_valid;
    } shader_pipeline;
};


// class Shader_clouds
Shader_clouds::Shader_clouds(void* graphics)
    : m_pimpl(std::make_unique<Impl>(*static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_clouds::~Shader_clouds() = default;

void Shader_clouds::signal_render_view_sizes_changed()
{
    m_pimpl->shader_pipeline.is_descriptor_set_valid = false;
}


void Shader_clouds::compute(Camera_internal const& camera, void* render_view_param)
{
    auto& p{ *m_pimpl };

    auto& render_view{ *static_cast<Graphics::Impl::Render_view_data*>(render_view_param) };
    assert(render_view.render_view_idx == 0);

    if (!p.shader_pipeline.is_descriptor_set_valid)
    {
        p.build_descriptor();
    }

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    // Ready needed images for compute shader.
    std::vector<std::pair<Vk_Image::Image*, VkImageLayout>> image_transitions;
    image_transitions.reserve(p.shader_pipeline.descriptor_set_images.size());
    for (auto* desc_set_image : p.shader_pipeline.descriptor_set_images)
    {
        image_transitions.emplace_back(desc_set_image, VK_IMAGE_LAYOUT_GENERAL);
    }

    Vk_Image::Image::transition_to(cmd, std::move(image_transitions));

    // Bind.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        p.shader_pipeline.pipeline_layout,
        0,
        1, &p.shader_pipeline.descriptor_set,
        0, nullptr);

    Shader_clouds_push_constants push_consts;
    glm_mat4_mul(
        const_cast<vec4*>(camera.get_calcd_cam_matrices()[render_view.render_view_idx].projection),
        const_cast<vec4*>(camera.get_calcd_cam_matrices()[render_view.render_view_idx].view),
        push_consts.inv_cam_projview);
    glm_mat4_inv(push_consts.inv_cam_projview, push_consts.inv_cam_projview);
    glm_vec3_copy(vec3{ -1000, 0, -1000 }, push_consts.voxel_grid_origin);
    // glm_vec3_copy(vec3{ 0, 0, 0 }, push_consts.voxel_grid_origin);
    push_consts.voxel_size = 8;

    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(Shader_clouds_push_constants),
                       &push_consts);

    // Run.
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

    k_cmd_dispatch_fn(cmd, p.g.clouds_image.get_extent(), p.thread_grp_sizes);

    // @NOTE: this gets transitioned before next image use, so no need for a
    //        `wait_until_completion()` step.
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
