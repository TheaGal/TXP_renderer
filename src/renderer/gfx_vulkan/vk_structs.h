#pragma once

#if TXP_GFX_BACKEND_VULKAN

#include <vulkan/vulkan.h>


namespace TXP
{
namespace Vk_Structs
{

VkRenderingAttachmentInfo txp_vk_attachment_info(VkImageView image_view,
                                                 VkClearValue* clear_value,
                                                 VkImageLayout image_layout);

VkRenderingInfo txp_vk_render_info(VkExtent2D render_extent,
                                   VkRenderingAttachmentInfo* color_attachment,
                                   VkRenderingAttachmentInfo* depth_attachment);

VkImageSubresourceRange txp_vk_image_subresource_range(VkImageAspectFlags aspect_mask);

VkSemaphoreSubmitInfo txp_vk_semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                                   VkSemaphore semaphore);

VkSubmitInfo2 txp_vk_submit_info(VkCommandBufferSubmitInfo* cmd_info,
                                 VkSemaphoreSubmitInfo* signal_info,
                                 VkSemaphoreSubmitInfo* wait_info);

}  // namespace Vk_Structs
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
