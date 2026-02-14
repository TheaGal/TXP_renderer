#if TXP_GFX_BACKEND_VULKAN

#include "vk_image.h"

// clang-format off
#include "vulkan/vulkan_core.h"
#include <vk_mem_alloc.h>
// clang-format on

#include "vk_structs.h"

#include <stdexcept>
#include <vector>


namespace TXP
{
namespace Vk_Image
{

/// class Image
Image::Image(VkImage img, VkImageAspectFlags aspect_mask)
    : m_img(img)
    , m_aspect_mask(aspect_mask)
{
}

void Image::transition_to(VkCommandBuffer cmd, VkImageLayout new_layout)
{
    // @NOTE: even if the old and new layouts are the same, run the pipeline barrier.

    VkImageMemoryBarrier2 image_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

        .oldLayout = m_current_layout,
        .newLayout = new_layout,

        .image = m_img,
        .subresourceRange = Vk_Structs::txp_vk_image_subresource_range(m_aspect_mask),
    };

    VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,

        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);

    m_current_layout = new_layout;
}

VkImage Image::get()
{
    return m_img;
}

VkImageLayout Image::get_layout()
{
    return m_current_layout;
}


/// class Allocated_image
/*static*/ void Allocated_image::set_vk_props(VkPhysicalDevice physical_device,
                                              VkDevice device,
                                              VmaAllocator allocator)
{
    s_physical_device = physical_device;
    s_device = device;
    s_allocator = allocator;
}

/*static*/ Allocated_image Allocated_image::create_image_2d(VkFormat format,
                                                            VkExtent2D extent,
                                                            VkImageUsageFlags usage_flags)
{
    if ((usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        throw std::runtime_error("Usage flags has depth stencil attachment.");

    return create_image(VK_IMAGE_TYPE_2D,
                        VK_IMAGE_VIEW_TYPE_2D,
                        format,
                        VkExtent3D{ extent.width, extent.height, 1 },
                        1,
                        1,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL,
                        usage_flags,
                        VK_IMAGE_ASPECT_COLOR_BIT);
}

/*static*/ Allocated_image Allocated_image::create_image_depth_buffer(VkExtent2D extent,
                                                                      VkImageUsageFlags usage_flags)
{
    if ((usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
        throw std::runtime_error("Usage flags doesn't have depth stencil attachment.");

    // Query what depth format is supported -- at least one of below is required to be supported.
    static std::vector<VkFormat> s_format_list{ VK_FORMAT_D32_SFLOAT_S8_UINT,  // Prefer SFLOAT.
                                                VK_FORMAT_D24_UNORM_S8_UINT }; // Fixed-point is okay too ig ¯\_(ツ)_/¯
    VkFormat depth_format{ VK_FORMAT_UNDEFINED };
    for (auto const& format : s_format_list)
    {
        VkFormatProperties2 format_properties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(s_physical_device, format, &format_properties);
        if (format_properties.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depth_format = format;
            break;
        }
    }
    if (depth_format == VK_FORMAT_UNDEFINED)
        throw std::runtime_error("This is a jank-ass video card and you should never get this error message.");

    // Create actual image.
    return create_image(VK_IMAGE_TYPE_2D,
                        VK_IMAGE_VIEW_TYPE_2D,
                        depth_format,
                        VkExtent3D{ extent.width, extent.height, 1 },
                        1,
                        1,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL,
                        usage_flags,
                        VK_IMAGE_ASPECT_DEPTH_BIT);
}

/*static*/ Allocated_image Allocated_image::create_image(VkImageType image_type,
                                                         VkImageViewType image_view_type,
                                                         VkFormat format,
                                                         VkExtent3D extent,
                                                         uint32_t mip_levels,
                                                         uint32_t array_layers,
                                                         VkSampleCountFlagBits msaa_samples,
                                                         VkImageTiling tiling,
                                                         VkImageUsageFlags usage_flags,
                                                         VkImageAspectFlags aspect_flags,
                                                         uint32_t view_base_mip_level /*= -1*/,
                                                         uint32_t view_mip_levels /*= -1*/,
                                                         uint32_t view_base_array_layer /*= -1*/,
                                                         uint32_t view_array_layers /*= -1*/)
{
    VkResult err;

    // Create image.
    Allocated_image new_img;
    {
        VkImageCreateInfo img_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .imageType = image_type,
            .format = format,
            .extent = extent,
            .mipLevels = mip_levels,
            .arrayLayers = array_layers,
            .samples = msaa_samples,
            .tiling = tiling,
            .usage = usage_flags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,  // @TODO: learn about different queue families and sharing mode.
            .pQueueFamilyIndices = &s_graphics_queue_family_idx,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo img_alloc_info{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };

        VkImage temp_vk_image;  // @NOTE: lvalue needed for `vmaCreateImage()`.

        err = vmaCreateImage(s_allocator,
                             &img_info,
                             &img_alloc_info,
                             &temp_vk_image,
                             &new_img.m_allocation,
                             nullptr);
        if (err)
            throw std::runtime_error("Image creation failed.");
        
        new_img.m_image = Image(std::move(temp_vk_image), aspect_flags);
        new_img.m_format = format;
        new_img.m_extent = extent;
    }

    // Create image view.
    VkImageViewCreateInfo img_view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = new_img.m_image.get(),
        .viewType = image_view_type,
        .format = format,
        // .components = ,  @CHECK: what is this? (I'm curious)
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = aspect_flags,
            .baseMipLevel   = (view_base_mip_level == -1 ? 0 : view_base_mip_level),
            .levelCount     = (view_mip_levels == -1 ? mip_levels : view_mip_levels),
            .baseArrayLayer = (view_base_array_layer == -1 ? 0 : view_base_array_layer),
            .layerCount     = (view_array_layers == -1 ? array_layers : view_array_layers),
        },
    };

    err = vkCreateImageView(s_device,
                            &img_view_info,
                            nullptr,
                            &new_img.m_image_view);
    if (err)
        throw std::runtime_error("Image view creation failed.");

    new_img.m_initialized = true;

    return new_img;
}

/// Default ctor (uninitialized.)
Allocated_image::Allocated_image()
    : m_image(Image{ nullptr, 0 })
{
}

void Allocated_image::teardown()
{
    vkDestroyImageView(s_device, m_image_view, nullptr);
    vmaDestroyImage(s_allocator, m_image.get(), m_allocation);
}

Image& Allocated_image::get_image()
{
    if (!m_initialized)
        throw std::runtime_error("Uninitialized Allocated_image.");
    return m_image;
}

VkImageView& Allocated_image::get_image_view()
{
    if (!m_initialized)
        throw std::runtime_error("Uninitialized Allocated_image.");
    return m_image_view;
}

VkExtent3D Allocated_image::get_extent()
{
    if (!m_initialized)
        throw std::runtime_error("Uninitialized Allocated_image.");
    return m_extent;
}

}  // namespace Vk_Structs
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
