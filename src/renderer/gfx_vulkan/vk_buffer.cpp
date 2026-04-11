#if TXP_GFX_BACKEND_VULKAN

#include "vk_buffer.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
// clang-format on

#include "btdatecheck.h"
#include "btlogger.h"

#include <cassert>
#include <stdexcept>


namespace TXP
{
namespace Vk_Buffer
{

Allocated_buffer::~Allocated_buffer()
{
    BT::date_deadline(2026, 4, 25);  // @TODO: figure this check for detecting undestroyed buffers deleted!!
    // if (m_created)
    // {
    //     BT_ERROR("Failed to destroy buffer before deletion.");
    //     assert(false);
    // }
}

void Allocated_buffer::create(VkDevice device,
                              VmaAllocator allocator,
                              VkDeviceSize buffer_size,
                              VkBufferUsageFlags buffer_usage_flags,
                              VmaAllocationCreateFlags buffer_allocation_flags)
{
    if (m_created)
        throw std::runtime_error("Trying to double create");

    m_used_allocator = allocator;

    VkBufferCreateInfo buffer_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                    .size = buffer_size,
                                    .usage = buffer_usage_flags };
    VmaAllocationCreateInfo buffer_alloc_info{ .flags = buffer_allocation_flags,
                                               .usage = VMA_MEMORY_USAGE_AUTO };
    VkResult err = vmaCreateBuffer(m_used_allocator,
                                   &buffer_info,
                                   &buffer_alloc_info,
                                   &m_buffer,
                                   &m_buffer_allocation,
                                   &m_buffer_allocation_info);
    if (err)
        throw std::runtime_error("Creation of buffer failed.");

    if (buffer_usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo buffer_bda_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = m_buffer,
        };
        m_device_address = vkGetBufferDeviceAddress(device, &buffer_bda_info);
    }

    m_created = true;
}

void Allocated_buffer::destroy()
{
    if (!m_created)
        throw std::runtime_error("Trying to destroy something not created.");

    vmaDestroyBuffer(m_used_allocator, m_buffer, m_buffer_allocation);
    m_created = false;
}

bool Allocated_buffer::is_created() const
{
    return m_created;
}

VkBuffer const& Allocated_buffer::get_buffer() const
{
    return m_buffer;
}

void* Allocated_buffer::get_p_mapped_data()
{
    return m_buffer_allocation_info.pMappedData;
}

VkDeviceAddress Allocated_buffer::get_device_address() const
{
    return m_device_address;
}

}  // namespace Vk_Buffer
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
