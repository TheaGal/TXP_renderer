#pragma once

#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

#include <GLFW/glfw3.h>
#include "VkBootstrap.h"

#define KHRONOS_STATIC 1
#include "ktx.h"
#include "ktxvulkan.h"
// clang-format on

#include "gfx_vulkan/vk_image.h"
#include "render_object/render_model.h"
#include "render_object/render_object.h"
#include "types.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <vector>


namespace TXP
{

struct Graphics::Impl
{
    Impl(std::string const& title, int32_t width, int32_t height)
        : window_title(title)
        , window_dims{ width, height }
    {
    }


    std::string window_title;
    GLFWwindow* window{ nullptr };

    int32_t window_dims[2];
    float_t monitor_scale{ 1.0f };


    void init_glfw_no_api();
    void init_window_props();
    void init_window();


    /// Holds Vulkan graphics initialization information.
    struct Vk_gfx_instance
    {
        vkb::Instance vkb_instance;
        vkb::Device vkb_device;

    #if defined(__APPLE__)
        // Apple lagging behind the standard and being a piece of shit wtf guys.  -Thea 2026/02/04
        static constexpr bool k_feature_draw_indirect_count{ false };
        static constexpr bool k_feature_minmax_sampler_filter{ false };
    #else
        static constexpr bool k_feature_draw_indirect_count{ true };
        static constexpr bool k_feature_minmax_sampler_filter{ true };
    #endif // defined(__APPLE__)

        VkInstance instance;
    #ifndef NDEBUG
        VkDebugUtilsMessengerEXT debug_utils_messenger;
    #endif
        VkSurfaceKHR surface;
        VkSurfaceFormatKHR surface_format;
        VkPhysicalDevice physical_device;
        VkPhysicalDeviceProperties physical_device_properties;
        VkDevice device;

        VmaAllocator allocator;

        VkSwapchainKHR swapchain;
        std::vector<Vk_Image::Image> swapchain_images;
        std::vector<VkImageView> swapchain_image_views;
        std::vector<VkSemaphore> swapchain_submit_semaphores;
        VkFormat swapchain_image_format;
        VkExtent2D swapchain_extent;

        VkQueue graphics_queue;
        uint32_t graphics_queue_family_idx;

        VkQueue async_compute_queue;
        uint32_t async_compute_queue_family_idx;

        VkQueue transfer_queue;
        uint32_t transfer_queue_family_idx;

        VkPipelineCache pipeline_cache{ VK_NULL_HANDLE };  // Unused for now.

        VkDescriptorPool imgui_desc_pool;
    };
    Vk_gfx_instance gfx;

    /// Number of frames-in-flight.
    static constexpr uint32_t k_frame_overlap{ 3 };

    /// Command buffer abstraction for this vulkan renderer.
    class Command_buffer
    {
    public:
        /// Allocates command buffer.
        void allocate(VkDevice device, VkCommandPool cmd_pool, bool is_primary_level)
        {
            VkCommandBufferAllocateInfo cmd_alloc_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = cmd_pool,
                .level = (is_primary_level ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                           : VK_COMMAND_BUFFER_LEVEL_SECONDARY),
                .commandBufferCount = 1,
            };

            VkResult err;

            err = vkAllocateCommandBuffers(device, &cmd_alloc_info, &m_cmd);
            if (err)
                throw std::runtime_error("Vulkan command pool allocation failed for frame #");
        }

        /// Resets command buffer, causing initialization for the next `.get()` call.
        void reset()
        {
            m_initialized = false;
        }

        /// Gets command buffer, initializing if needed.
        VkCommandBuffer get()
        {
            if (!m_initialized)
            {
                VkResult err;

                err = vkResetCommandBuffer(m_cmd, 0);
                if (err)
                {
                    std::runtime_error("Reset command buffer failed.");
                }

                VkCommandBufferBeginInfo info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
                };

                //start the command buffer recording
                err = vkBeginCommandBuffer(m_cmd, &info);
                if (err)
                {
                    std::runtime_error("Begin command buffer failed.");
                }

                m_initialized = true;
            }

            return m_cmd;
        }

        /// Ends command buffer recording.
        void finish()
        {
            VkResult err;

            err = vkEndCommandBuffer(m_cmd);
            if (err)
            {
                std::runtime_error("End command buffer failed.");
            }
        }

    private:
        bool m_initialized{ false };
        VkCommandBuffer m_cmd;
    };

    // @TODO: @THEA: move this over to a vk-buffers!!!
    class Allocated_buffer
    {
    public:
        /// Creates buffer.
        void create(VkDevice device,
                    VmaAllocator allocator,
                    VkDeviceSize buffer_size,
                    VkBufferUsageFlags buffer_usage_flags,
                    VmaAllocationCreateFlags buffer_allocation_flags)
        {
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
        }

        /// Destroys the created buffer.
        void destroy()
        {
            vmaDestroyBuffer(m_used_allocator, m_buffer, m_buffer_allocation);
        }

        VkBuffer const& get_buffer() const
        {
            return m_buffer;
        }

        /// Gets the `.pMappedData` pointer.
        void* get_p_mapped_data()
        {
            return m_buffer_allocation_info.pMappedData;
        }

        /// Gets the buffer device address, if it was initialized with that.
        VkDeviceAddress get_device_address() const
        {
            return m_device_address;
        }

    private:
        VmaAllocator m_used_allocator;
        VkBuffer m_buffer;
        VmaAllocation m_buffer_allocation;
        VmaAllocationInfo m_buffer_allocation_info;
        VkDeviceAddress m_device_address{ 0xdeadbeef };
    };


    /// Holds per-frame data.
    struct Frame_data
    {
        VkCommandPool command_pool;
        Command_buffer graphics_queue_command_buffer;
        VkSemaphore acquire_nxt_img_semaphore;
        VkFence render_fence;

        std::vector<Allocated_buffer> environment_data_buffers;  // Matches number of render views.
        Allocated_buffer model_transform_set_buffer;
    };
    std::array<Frame_data, k_frame_overlap> frames;

    /// Index of current frame.
    size_t current_frame_idx{ 0 };
    uint32_t current_swapchain_image_idx;

    Frame_data& get_current_frame();
    uint32_t get_current_frame_idx();
    Vk_Image::Image& get_current_swapchain_image();
    VkImageView get_current_swapchain_image_view();
    VkSemaphore get_current_swapchain_submit_semaphore();

    /// HDR draw image (main geometry pipeline).
    struct Render_view_hdr_image
    {
        size_t render_view_idx;
        Vk_Image::Allocated_image color;
        Vk_Image::Allocated_image depth;
    };
    std::vector<Render_view_hdr_image> render_view_hdr_images;

    /// Helper for loading shader module.
    VkShaderModule load_shader_module(std::string const& fname)
    {   // Open file.
        std::ifstream f(fname, std::ios::ate | std::ios::binary);

        if (!f.is_open())
            throw std::runtime_error("Failed to open file: \"" + fname + "\"");

        // Read file.
        size_t file_size{ static_cast<size_t>(f.tellg()) };
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

        f.seekg(0);
        f.read(reinterpret_cast<char*>(buffer.data()), file_size);

        f.close();

        // Create new shader module.
        VkShaderModuleCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = (buffer.size() * sizeof(uint32_t)),
            .pCode = buffer.data(),
        };

        VkShaderModule shader_module;
        VkResult err = vkCreateShaderModule(gfx.device, &info, nullptr, &shader_module);
        if (err)
            throw std::runtime_error("Create shader \"" + fname + "\" failed.");

        return shader_module;
    }


    void init_vulkan_instance();
    void init_vulkan_window_surface();
    void init_vulkan_build_device();
    void init_vulkan_create_memory_allocator();
    void select_vulkan_window_surface_format();
    void init_vulkan_build_swapchain();
    void init_vulkan_retrieve_queues();
    void init_vulkan_create_cmd_structures();
    void init_vulkan_create_sync_structures();
    void init_vulkan_for_imgui();
    void init_vulkan_render_graph_resources();
    void init_vulkan_create_descriptors();


    /// Adds textures.
    struct Texture_entry
    {
        ktxVulkanTexture texture;
        VkImageView image_view;
        VkSampler sampler;
        size_t gpu_idx;
    };
    std::unordered_map<std::string, Texture_entry> texture_entries;  // @TODO: delete all ktx vk textures. (use `ktxVulkanTexture_Destruct()`)  Also delete all image views and samplers.

    ktxVulkanDeviceInfo ktx_vk_device_info;

    void construct_ktx_vk_device_info();
    void destruct_ktx_vk_device_info();

    ktxVulkanTexture load_and_upload_texture(std::string const& fname);
    void add_texture_entry(std::string const& texture_name, ktxVulkanTexture&& allocated_image);

    /// Add models.
    void upload_model_entries_to_gpu(Render_model_data_collection& data_collection);

    struct Model_buffer
    {
        Allocated_buffer vertex_index_buffer;  // Vertex part first, index part second.
        VkDeviceSize offset_to_index_buffer;

        void bind(VkCommandBuffer cmd)
        {
            VkDeviceSize offset{ 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_index_buffer.get_buffer(), &offset);
            vkCmdBindIndexBuffer(cmd,
                                 vertex_index_buffer.get_buffer(),
                                 offset_to_index_buffer,
                                 VK_INDEX_TYPE_UINT32);
        }
    };
    Model_buffer combined_static_model;

    /// Descriptor type information.
    struct Descriptor_type_info
    {
        VkDescriptorType descriptor_type;
        bool use_variable_descriptor_count_binding_flag{ false };
        uint32_t variable_descriptor_count;
    };

    /// Descriptor binding types.
    using Descriptor_binding_set_t = std::vector<std::pair<uint32_t, Descriptor_type_info>>;

    /// Reflection data to descriptor bindings helper.
    std::vector<Descriptor_binding_set_t> get_descriptor_binding_sets_from_shader_properties(
        Shader_Creation::Extracted_info const& info,
        Shader_Creation::Shader_pipeline_type type);

    VkShaderStageFlags get_stage_flags_from_shader_type(Shader_Creation::Shader_pipeline_type type);

    /// Build descriptor layouts.
    VkDescriptorSetLayout build_descriptor_layout(
        Descriptor_binding_set_t&& bindings,
        VkShaderStageFlags shader_stages,
        VkDescriptorSetLayoutCreateFlags flags);
    
    /// Allocate descriptors.
    class Descriptor_allocator
    {
    public:
        /// Pool size ratios.
        using Pool_size_ratio = std::pair<VkDescriptorType, float_t>;

        /// Initialize.
        void init_pool(VkDevice device,
                       VmaAllocator allocator,
                       uint32_t max_sets,
                       std::vector<Pool_size_ratio>&& size_ratios)
        {
            m_device = device;
            m_allocator = allocator;

            std::vector<VkDescriptorPoolSize> pool_sizes;
            pool_sizes.reserve(size_ratios.size());

            for (auto [ratio_type, ratio_size] : size_ratios)
            {
                pool_sizes.emplace_back(VkDescriptorPoolSize{
                    .type = ratio_type,
                    .descriptorCount = static_cast<uint32_t>(ratio_size * max_sets),
                });
            }

            VkDescriptorPoolCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .maxSets = max_sets,
                .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
                .pPoolSizes = pool_sizes.data(),
            };
            vkCreateDescriptorPool(m_device, &info, nullptr, &m_pool);
        }

        /// Tears down pool.
        void teardown_pool()
        {
            vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        }

        /// Clears all allocated descriptors.
        void clear_pool()
        {
            vkResetDescriptorPool(m_device, m_pool, 0);
        }

        /// Allocates a single descriptor set. Useful for per-frame descriptor sets.
        VkDescriptorSet allocate(VkDescriptorSetLayout layout)
        {
            VkDescriptorSetAllocateInfo info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = m_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
            };

            VkDescriptorSet set;
            VkResult err = vkAllocateDescriptorSets(m_device, &info, &set);

            if (err)
                throw std::runtime_error("Allocating descriptor set failed.");

            return set;
        }

        /// Allocates a descriptor set with variable descriptor count information.
        VkDescriptorSet allocate(VkDescriptorSetLayout layout, uint32_t variable_descriptor_count)
        {
            VkDescriptorSetVariableDescriptorCountAllocateInfo variable_desc_count_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                .descriptorSetCount = 1,
                .pDescriptorCounts = &variable_descriptor_count,
            };
            VkDescriptorSetAllocateInfo info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = &variable_desc_count_info,
                .descriptorPool = m_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
            };

            VkDescriptorSet set;
            VkResult err = vkAllocateDescriptorSets(m_device, &info, &set);

            if (err)
                throw std::runtime_error("Allocating descriptor set failed.");

            return set;
        }

    private:
        VkDevice m_device;
        VmaAllocator m_allocator;
        VkDescriptorPool m_pool;
    };

    Descriptor_allocator global_descriptor_allocator;

#if 0
    VkDescriptorSet draw_image_descriptors;               // 3rd (just has to be after 1st)
    VkDescriptorSetLayout draw_image_descriptor_layout;   // 1st (multiple ones)
    VkPipeline draw_image_compute_pipeline;               // 4th (just has to be after 2nd)
    VkPipelineLayout draw_image_compute_pipeline_layout;  // 2nd
#endif // 0

#if 0
    /// Adds shader pipelines.
    struct Shader_pipeline
    {
        std::vector<VkDescriptorSetLayout> descriptor_layouts;
        VkPipelineLayout pipeline_layout;
        std::vector<VkDescriptorSet> descriptor_sets;
        VkPipeline pipeline;
    };
    std::unordered_map<std::string, Shader_pipeline> shader_pipelines;
#endif // 0


    /// Polls window for input events.
    void poll_input_events();

    /// Callback for imgui draw.
    std::function<void()> imgui_build_contents_callback;

    void build_imgui_contents(std::vector<Render_view_size>& out_rend_view_sizes);

    /// Sets render view sizes and rebuilds any needed render views if changed.
    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);


    void* start_new_frame(size_t rend_view_idx);

    void blit_image(Vk_Image::Image& from_image,
                    VkExtent3D from_extent,
                    Vk_Image::Image& to_image,
                    VkExtent3D to_extent);
    void render_imgui();
    void present_frame_to_screen();

    void wait_until_gpu_idle();
};

}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
