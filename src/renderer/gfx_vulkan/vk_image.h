#pragma once

#if TXP_GFX_BACKEND_VULKAN

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
// clang-format on

#include <utility>
#include <vector>


namespace TXP
{
namespace Vk_Image
{

/// Image abstraction for vulkan renderer.
class Image
{
public:
    Image(VkImage img, VkImageAspectFlags aspect_mask);

    /// Transitions the image to the `new_layout`.
    /// @note even if the old and new layouts are the same, the pipeline barrier is still run for
    ///       the sake of memory synchronization.
    static void transition_to(VkCommandBuffer cmd,
                              std::vector<std::pair<Image*, VkImageLayout>>&& new_layouts);

    VkImage get();

    VkImageLayout get_layout();

private:
    VkImage m_img;
    VkImageLayout m_current_layout{ VK_IMAGE_LAYOUT_UNDEFINED };
    VkImageAspectFlags m_aspect_mask{ 0 };
};

/// Allocated image abstraction for vulkan renderer.
class Allocated_image
{
public:
    static void set_vk_props(VkPhysicalDevice physical_device,
                             VkDevice device,
                             VmaAllocator allocator);

    static Allocated_image create_image_2d(VkFormat format,
                                           VkExtent2D extent,
                                           VkImageUsageFlags usage_flags);

    static Allocated_image create_image_depth_buffer(VkExtent2D extent);

    static Allocated_image create_image(VkImageType image_type,
                                        VkImageViewType image_view_type,
                                        VkFormat format,
                                        VkExtent3D extent,
                                        uint32_t mip_levels,
                                        uint32_t array_layers,
                                        VkSampleCountFlagBits msaa_samples,
                                        VkImageTiling tiling,
                                        VkImageUsageFlags usage_flags,
                                        VkImageAspectFlags aspect_flags,
                                        uint32_t view_base_mip_level = -1,
                                        uint32_t view_mip_levels = -1,
                                        uint32_t view_base_array_layer = -1,
                                        uint32_t view_array_layers = -1);

    /// Default ctor (uninitialized.)
    Allocated_image();

    void teardown();

    Image& get_image();

    VkImageView& get_image_view();

    VkExtent3D get_extent() const;

    VkFormat get_format() const;

private:
    inline static VkPhysicalDevice s_physical_device;
    inline static VkDevice s_device;
    inline static VmaAllocator s_allocator;
    inline static uint32_t s_graphics_queue_family_idx;
    inline static uint32_t s_async_compute_queue_family_idx;
    inline static uint32_t s_transfer_queue_family_idx;

    bool m_initialized{ false };
    Image m_image;
    VkImageView m_image_view;
    VmaAllocation m_allocation;
    VkExtent3D m_extent;
    VkFormat m_format;
};

}  // namespace Vk_Structs
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
