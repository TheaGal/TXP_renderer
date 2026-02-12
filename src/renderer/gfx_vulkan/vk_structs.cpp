#if TXP_GFX_BACKEND_VULKAN

#include "vk_structs.h"


namespace TXP
{

VkRenderingAttachmentInfo Vk_Structs::txp_vk_attachment_info(VkImageView image_view,
                                                             VkClearValue* clear_value,
                                                             VkImageLayout image_layout)
{
    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,

        .imageView = image_view,
        .imageLayout = image_layout,
        .loadOp = (clear_value ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD),
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    if (clear_value != nullptr)
    {
        color_attachment.clearValue = *clear_value;
    }

    return color_attachment;
}

VkRenderingInfo Vk_Structs::txp_vk_render_info(VkExtent2D render_extent,
                                               VkRenderingAttachmentInfo* color_attachment,
                                               VkRenderingAttachmentInfo* depth_attachment)
{
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,

        .renderArea = VkRect2D{ .offset = { 0, 0 },
                                .extent = render_extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = color_attachment,
        .pDepthAttachment = depth_attachment,
        .pStencilAttachment = nullptr,
    };

    return rendering_info;
}

VkImageSubresourceRange Vk_Structs::txp_vk_image_subresource_range(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange subresource_range{
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    return subresource_range;
}

VkSemaphoreSubmitInfo Vk_Structs::txp_vk_semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                                               VkSemaphore semaphore)
{
    VkSemaphoreSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stage_mask,
        .deviceIndex = 0,
    };

    return submit_info;
}

VkSubmitInfo2 Vk_Structs::txp_vk_submit_info(VkCommandBufferSubmitInfo* cmd_info,
                                             VkSemaphoreSubmitInfo* signal_info,
                                             VkSemaphoreSubmitInfo* wait_info)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .waitSemaphoreInfoCount = (wait_info == nullptr ? 0u : 1u),
        .pWaitSemaphoreInfos = wait_info,

        .commandBufferInfoCount = 1u,
        .pCommandBufferInfos = cmd_info,

        .signalSemaphoreInfoCount = (signal_info == nullptr ? 0u : 1u),
        .pSignalSemaphoreInfos = signal_info,
    };
    return info;
}

}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
