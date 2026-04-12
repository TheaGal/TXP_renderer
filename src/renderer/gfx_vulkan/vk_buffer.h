#pragma once

#if TXP_GFX_BACKEND_VULKAN

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
// clang-format on


namespace TXP
{
namespace Vk_Buffer
{

// Buffer on GPU or CPU side that holds a fixed amount of memory.
class Allocated_buffer
{
public:
    ~Allocated_buffer();

    /// Creates buffer.
    void create(VkDevice device,
                VmaAllocator allocator,
                VkDeviceSize buffer_size,
                VkBufferUsageFlags buffer_usage_flags,
                VmaAllocationCreateFlags buffer_allocation_flags);

    /// Destroys the created buffer.
    void destroy();

    /// Checks whether the buffer is created.
    bool is_created() const;

    /// Enables or disables `is_created` check upon deletion.
    void set_created_check(bool do_check);

    /// Gets buffer.
    VkBuffer const& get_buffer() const;

    /// Gets the `.pMappedData` pointer.
    void* get_p_mapped_data();

    /// Gets the buffer device address, if it was initialized with that.
    VkDeviceAddress get_device_address() const;

private:
    bool m_created{ false };
    bool m_check_whether_created{ true };
    VmaAllocator m_used_allocator;
    VkBuffer m_buffer;
    VmaAllocation m_buffer_allocation;
    VmaAllocationInfo m_buffer_allocation_info;
    VkDeviceAddress m_device_address{ 0xdeadbeef };
};

}  // namespace Vk_Buffer
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
