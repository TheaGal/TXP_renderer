#include "vulkan/vulkan_core.h"
#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include "VkBootstrap.h"
// clang-format on

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>


namespace TXP
{

VkImageSubresourceRange txp_vk_image_subresource_range(VkImageAspectFlags aspect_mask)
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

VkSemaphoreSubmitInfo txp_vk_semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                                   VkSemaphore semaphore)
{
    VkSemaphoreSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .stageMask = stage_mask,
        .deviceIndex = 0,
        .value = 1,
    };

	return submit_info;
}

VkSubmitInfo2 txp_vk_submit_info(VkCommandBufferSubmitInfo* cmd_info,
                                 VkSemaphoreSubmitInfo* signal_info,
                                 VkSemaphoreSubmitInfo* wait_info)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .waitSemaphoreInfoCount = (wait_info == nullptr ? 0u : 1u),
        .pWaitSemaphoreInfos = wait_info,

        .signalSemaphoreInfoCount = (signal_info == nullptr ? 0u : 1u),
        .pSignalSemaphoreInfos = signal_info,

        .commandBufferInfoCount = 1u,
        .pCommandBufferInfos = cmd_info,
    };
    return info;
}

struct Graphics::Impl
{
    Impl(std::string const& title, int32_t width, int32_t height)
        : window_title(title)
        , render_dim{ width, height }
    {
    }


    std::string window_title;
    GLFWwindow* window{ nullptr };

    int32_t render_dim[2];


    void init_glfw_no_api();
    void init_window_props();
    void init_window();


    /// Image abstraction for vulkan renderer.
    class Image
    {
    public:
        Image(VkImage img)
            : m_img(img)
        {
        }

        void transition_to(VkCommandBuffer cmd, VkImageLayout new_layout)
        {
            if (m_current_layout == new_layout)
                return;  // Cancel transition if same layout.

            VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                                  ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                  : VK_IMAGE_ASPECT_COLOR_BIT);

            VkImageMemoryBarrier2 image_barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,

                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

                .oldLayout = m_current_layout,
                .newLayout = new_layout,

                .subresourceRange = txp_vk_image_subresource_range(aspect_mask),
                .image = m_img,
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

        VkImage get()
        {
            return m_img;
        }

        VkImageLayout get_layout()
        {
            return m_current_layout;
        }

    private:
        VkImage m_img;
        VkImageLayout m_current_layout{ VK_IMAGE_LAYOUT_UNDEFINED };
    };

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
        VkPhysicalDevice physical_device;
        VkPhysicalDeviceProperties physical_device_properties;
        VkDevice device;

        VmaAllocator allocator;

        VkSwapchainKHR swapchain;
        std::vector<Image> swapchain_images;
        std::vector<VkImageView> swapchain_image_views;
        VkFormat swapchain_image_format;
        VkExtent2D swapchain_extent;

        VkQueue graphics_queue;
        uint32_t graphics_queue_family_idx;

        VkQueue async_compute_queue;
        uint32_t async_compute_queue_family_idx;

        VkQueue transfer_queue;
        uint32_t transfer_queue_family_idx;
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
            {
                throw std::runtime_error("Vulkan command pool allocation failed for frame #");
            }
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
                    .pInheritanceInfo = nullptr,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
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

    /// Holds per-frame data.
    struct Frame_data
    {
        VkCommandPool command_pool;
        Command_buffer graphics_queue_command_buffer;
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence render_fence;

        // @TODO: figure out the vv below vv
        // vk_buffer::Allocated_buffer camera_buffer;
        // vk_buffer::GPU_geo_per_frame_buffer geo_per_frame_buffer;
    };
    std::array<Frame_data, k_frame_overlap> frames;


    void init_vulkan_instance();
    void init_vulkan_window_surface();
    void init_vulkan_build_device();
    void init_vulkan_create_memory_allocator();
    void init_vulkan_build_swapchain();
    void init_vulkan_retrieve_queues();
    void init_vulkan_create_cmd_structures();
    void init_vulkan_create_sync_structures();
    void init_vulkan_allocate_descriptors();
    void init_vulkan_create_pipelines();


    /// Index of current frame.
    size_t current_frame_idx{ (size_t)-1 };
    uint32_t current_swapchain_image_idx;

    void start_new_frame();

    void render_incomplete_jojojojojojs();

    void present_frame_to_screen();
};


// struct Graphics::Impl
void Graphics::Impl::init_glfw_no_api()
{
    auto result = glfwInit();
    assert(result == GLFW_TRUE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Graphics::Impl::init_window_props()
{
    assert(render_dim[0] > 0 && render_dim[1] > 0);

    // Apply centering hints.
    struct Monitor_workarea
    {
        int32_t xpos;
        int32_t ypos;
        int32_t width;
        int32_t height;
    } monitor_workarea;
    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(),  // monitor_ptr,
                           &monitor_workarea.xpos,
                           &monitor_workarea.ypos,
                           &monitor_workarea.width,
                           &monitor_workarea.height);

    int32_t centered_window_pos[2]{
        monitor_workarea.xpos +
            static_cast<int32_t>(monitor_workarea.width * 0.5 - render_dim[0] * 0.5),
        monitor_workarea.ypos +
            static_cast<int32_t>(monitor_workarea.height * 0.5 - render_dim[1] * 0.5),
    };

    glfwWindowHint(GLFW_POSITION_X, centered_window_pos[0]);
    glfwWindowHint(GLFW_POSITION_Y, centered_window_pos[1]);

    // @TODO: implement vv below vv
    // glfwWindowHint(GLFW_RESIZABLE, app_window_settings.is_resizable ? GLFW_TRUE : GLFW_FALSE);
    // glfwWindowHint(GLFW_DECORATED, app_window_settings.has_border ? GLFW_TRUE : GLFW_FALSE);
    // glfwWindowHint(GLFW_MAXIMIZED, app_window_settings.is_maximized ? GLFW_TRUE : GLFW_FALSE);

    // submit_window_dims(win_dims.width, win_dims.height);  // @TODO

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
}

void Graphics::Impl::init_window()
{
    assert(render_dim[0] > 0 && render_dim[1] > 0);
    assert(window == nullptr);

    window = glfwCreateWindow(render_dim[0],
                              render_dim[1],
                              window_title.c_str(),
                              nullptr,
                              nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Window creation failed.");
    }

    // @TODO: add window icon here.


    glfwShowWindow(window);

    // // Window callbacks.
    // // @NOTE: With key callbacks etc that's also used by Imgui, Imgui
    // //   chains these callbacks so they don't get lost.
    // glfwSetKeyCallback(m_window, key_callback);
    // glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    // glfwSetCursorPosCallback(m_window, cursor_position_callback);
    // glfwSetScrollCallback(m_window, scroll_callback);
    // glfwSetWindowFocusCallback(m_window, window_focus_callback);
    // glfwSetWindowIconifyCallback(m_window, window_iconify_callback);
}

void Graphics::Impl::init_vulkan_instance()
{   // Build vulkan instance (targeting Vulkan 1.3).
    vkb::InstanceBuilder builder;
    vkb::Result<vkb::Instance> instance_build_result{
        builder
            .set_app_name("Thea Cross-Platform (TXP) Renderer")
            .require_api_version(1, 3, 0)
        #ifndef NDEBUG
            .request_validation_layers(true)
            .use_default_debug_messenger()
        #endif
            .build()
    };
    if (!instance_build_result.has_value())
    {
        throw std::runtime_error("Vulkan instance creation failed.");
    }

    gfx.vkb_instance = instance_build_result.value();
    gfx.instance = gfx.vkb_instance.instance;
#ifndef NDEBUG
    gfx.debug_utils_messenger = gfx.vkb_instance.debug_messenger;
#endif
}

void Graphics::Impl::init_vulkan_window_surface()
{
    VkResult err = glfwCreateWindowSurface(gfx.instance, window, nullptr, &gfx.surface);
    if (err)
    {
        throw std::runtime_error("Vulkan surface creation failed.");
    }
}

void Graphics::Impl::init_vulkan_build_device()
{   // Vulkan 1.3 features.
    VkPhysicalDeviceVulkan13Features vulkan13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    // Vulkan 1.2 features.
    VkPhysicalDeviceVulkan12Features vulkan12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        // For non-uniform, dynamic arrays of textures in shaders.
        .descriptorIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        // For buffer references in the stead of descriptor sets.
        .bufferDeviceAddress = VK_TRUE,
    };

    if constexpr (Vk_gfx_instance::k_feature_draw_indirect_count)
        vulkan12_features.drawIndirectCount = VK_TRUE;  // For `vkCmdDrawIndexedIndirectCount`.
    if constexpr (Vk_gfx_instance::k_feature_minmax_sampler_filter)
        vulkan12_features.samplerFilterMinmax = VK_TRUE;  // For MIN/MAX sampler when creating mip chains for occlusion culling.

    // Select physical device.
    vkb::PhysicalDeviceSelector selector{ gfx.vkb_instance };
    vkb::PhysicalDevice physical_device;
    {
        auto physical_device_selection{
            selector
                .set_minimum_version(1, 3)
                .set_required_features_13(vulkan13_features)
                .set_required_features_12(vulkan12_features)
                .set_surface(gfx.surface)
                .set_required_features({
                    // @NOTE: @FEATURES: Enable required features right here
                    // .multiDrawIndirect = VK_TRUE,         // So that vkCmdDrawIndexedIndirect() can be called with a >1 drawCount. (@NOTE: not happening with current setup)
                    .depthClamp = VK_TRUE,                // For shadow maps, this is really nice.
                    .fillModeNonSolid = VK_TRUE,          // To render wireframes.
                    .samplerAnisotropy = VK_TRUE,
                    .fragmentStoresAndAtomics = VK_TRUE,  // For the picking buffer! @TODO: If a release build then disable.
                })
                .select()
        };

        if (!physical_device_selection.has_value())
        {
            std::ostringstream err_msg;
            err_msg << "No physical device found that meets requirements.\n";
            for (auto const& det_msg : physical_device_selection.detailed_failure_reasons())
                err_msg << "  " << det_msg << "\n";

            throw std::runtime_error(err_msg.str());
        }

        physical_device = physical_device_selection.value();
    }
    gfx.physical_device = physical_device.physical_device;
    gfx.physical_device_properties = physical_device.properties;

    // @TODO: import bttrace and print the vv below vv out!
    // // Print phsyical device properties.
    // constexpr uint32_t k_built_sdk_version{ VK_HEADER_VERSION_COMPLETE };
    // std::cout << "-=-=- Chosen Physical Device Properties -=-=-" << std::endl;
    // std::cout << "BUILT_SDK_VERSION                 : " << VK_API_VERSION_MAJOR(k_built_sdk_version) << "." << VK_API_VERSION_MINOR(k_built_sdk_version) << "." << VK_API_VERSION_PATCH(k_built_sdk_version) << "." << VK_API_VERSION_VARIANT(k_built_sdk_version) << std::endl;
    // std::cout << "API_VERSION                       : " << VK_API_VERSION_MAJOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_MINOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_PATCH(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_VARIANT(out_physical_device_properties.apiVersion) << std::endl;
    // std::cout << "DRIVER_VERSION(raw)               : " << out_physical_device_properties.driverVersion << std::endl;
    // std::cout << "VENDOR_ID                         : " << out_physical_device_properties.vendorID << std::endl;
    // std::cout << "DEVICE_ID                         : " << out_physical_device_properties.deviceID << std::endl;
    // std::cout << "DEVICE_TYPE                       : " << out_physical_device_properties.deviceType << std::endl;
    // std::cout << "DEVICE_NAME                       : " << out_physical_device_properties.deviceName << std::endl;
    // std::cout << "MAX_IMAGE_DIMENSION_1D            : " << out_physical_device_properties.limits.maxImageDimension1D << std::endl;
    // std::cout << "MAX_IMAGE_DIMENSION_2D            : " << out_physical_device_properties.limits.maxImageDimension2D << std::endl;
    // std::cout << "MAX_IMAGE_DIMENSION_3D            : " << out_physical_device_properties.limits.maxImageDimension3D << std::endl;
    // std::cout << "MAX_IMAGE_DIMENSION_CUBE          : " << out_physical_device_properties.limits.maxImageDimensionCube << std::endl;
    // std::cout << "MAX_IMAGE_ARRAY_LAYERS            : " << out_physical_device_properties.limits.maxImageArrayLayers << std::endl;
    // std::cout << "MAX_SAMPLER_ANISOTROPY            : " << out_physical_device_properties.limits.maxSamplerAnisotropy << std::endl;
    // std::cout << "MAX_BOUND_DESCRIPTOR_SETS         : " << out_physical_device_properties.limits.maxBoundDescriptorSets << std::endl;
    // std::cout << "MINIMUM_BUFFER_ALIGNMENT          : " << out_physical_device_properties.limits.minUniformBufferOffsetAlignment << std::endl;
    // std::cout << "MAX_COLOR_ATTACHMENTS             : " << out_physical_device_properties.limits.maxColorAttachments << std::endl;
    // std::cout << "MAX_DRAW_INDIRECT_COUNT           : " << out_physical_device_properties.limits.maxDrawIndirectCount << std::endl;
    // std::cout << "MAX_DESCRIPTOR_SET_SAMPLED_IMAGES : " << out_physical_device_properties.limits.maxDescriptorSetSampledImages << std::endl;
    // std::cout << "MAX_DESCRIPTOR_SET_SAMPLERS       : " << out_physical_device_properties.limits.maxDescriptorSetSamplers << std::endl;
    // std::cout << "MAX_SAMPLER_ALLOCATION_COUNT      : " << out_physical_device_properties.limits.maxSamplerAllocationCount << std::endl;
    // std::cout << std::endl;

    // Build Vulkan device.
    vkb::DeviceBuilder device_builder{ physical_device };
    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };
    gfx.vkb_device =
        device_builder
            .add_pNext(&shader_draw_parameters_features)
            .build()
            .value();
    gfx.device = gfx.vkb_device.device;
}

void Graphics::Impl::init_vulkan_create_memory_allocator()
{   // Initialize VMA.
    VmaAllocatorCreateInfo vma_allocator_info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,  // To access GPU pointers.
        .physicalDevice = gfx.physical_device,
        .device = gfx.device,
        .instance = gfx.instance,
    };
    vmaCreateAllocator(&vma_allocator_info, &gfx.allocator);
}

void Graphics::Impl::init_vulkan_build_swapchain()
{   // Build swapchain.
    vkb::SwapchainBuilder swapchain_builder{ gfx.physical_device, gfx.device, gfx.surface };
    vkb::Swapchain swapchain{
        swapchain_builder.use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // Mailbox (G-Sync/Freesync).
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)    // FIFO (V-Sync).
            .set_desired_extent(render_dim[0], render_dim[1])
            // @TODO: TRANSFER_DST image usage added below. Try removing once renderer is finished
            // (assuming you're not gonna have some kind of image transfer as the last step into the
            // swapchain image).
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value()
    };

    gfx.swapchain = swapchain.swapchain;

    gfx.swapchain_images = ([&]() {
        auto swapchain_imgs_val = swapchain.get_images().value();

        std::vector<Image> images;
        images.reserve(swapchain_imgs_val.size());

        for (auto img : swapchain_imgs_val)
            images.emplace_back(img);
        
        return images;
    })();

    gfx.swapchain_image_views = swapchain.get_image_views().value();
    gfx.swapchain_image_format = swapchain.image_format;
    gfx.swapchain_extent.width = render_dim[0];
    gfx.swapchain_extent.height = render_dim[1];
}

void Graphics::Impl::init_vulkan_retrieve_queues()
{
    gfx.graphics_queue = gfx.vkb_device.get_queue(vkb::QueueType::graphics).value();
    gfx.graphics_queue_family_idx = gfx.vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // @THEA: getting more queues will be a bit more complicated. It seems that on the M4 pro chip
    //   there are 4 queue families which all have complete capabilities, so using the `compute` or
    //   `transfer` flags will actually not find them!
    // @TODO: figure out some way to get multiple queues as long as they have one of the wanted
    //   capabilities. ALSO, would be good to really think about the queue architecture for this to
    //   really shine.  -Thea 2026/02/04

    // @NOTE: the vv below vv queues are left unused currently.

    // gfx.async_compute_queue = gfx.vkb_device.get_queue(vkb::QueueType::compute).value();
    // gfx.async_compute_queue_family_idx = gfx.vkb_device.get_queue_index(vkb::QueueType::compute).value();

    // gfx.transfer_queue = gfx.vkb_device.get_queue(vkb::QueueType::transfer).value();
    // gfx.transfer_queue_family_idx = gfx.vkb_device.get_queue_index(vkb::QueueType::transfer).value();
}

void Graphics::Impl::init_vulkan_create_cmd_structures()
{
    VkResult err;

    VkCommandPoolCreateInfo cmd_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = gfx.graphics_queue_family_idx,
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateCommandPool(gfx.device, &cmd_pool_info, nullptr, &frames[i].command_pool);
        if (err)
        {
            throw std::runtime_error("Vulkan command pool creation failed for frame #");
        }

        frames[i].graphics_queue_command_buffer.allocate(gfx.device,
                                                         frames[i].command_pool,
                                                         true);
    }
}

void Graphics::Impl::init_vulkan_create_sync_structures()
{
    VkResult err;

    // Create fence to sync when gpu has finished rendering the frame.
    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  // For waiting on it for the first frame.
    };

    // Create semaphores for syncing swapchain rendering.
    VkSemaphoreCreateInfo semaphore_create_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateFence(gfx.device, &fence_create_info, nullptr, &frames[i].render_fence);
        if (err)
        {
            throw std::runtime_error("Vulkan render fence creation failed for frame #");
        }

        err = vkCreateSemaphore(gfx.device, &semaphore_create_info, nullptr, &frames[i].swapchain_semaphore);
        if (err)
        {
            throw std::runtime_error("Vulkan swapchain semaphore creation failed for frame #");
        }

        err = vkCreateSemaphore(gfx.device, &semaphore_create_info, nullptr, &frames[i].render_semaphore);
        if (err)
        {
            throw std::runtime_error("Vulkan render semaphore creation failed for frame #");
        }
    }
}

void Graphics::Impl::init_vulkan_allocate_descriptors()
{
    // @TODO: figure out the descriptor sets vv below vv
    // // Init allocator pool.
    // std::vector<vk_desc::Descriptor_allocator::Pool_size_ratio> sizes{
    //     { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    // };

    // out_descriptor_alloc.init_pool(device, 10, sizes);

    // // Build layout.
    // vk_desc::Descriptor_layout_builder builder;
    // builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    // out_descriptor_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

    // // Allocate descriptor set.
    // out_descriptor_set = out_descriptor_alloc.allocate(device, out_descriptor_layout);

    // VkDescriptorImageInfo image_info{
    //     .imageView = hdr_image_view,
    //     .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    // };

    // VkWriteDescriptorSet image_write{
    //     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    //     .pNext = nullptr,
    //     .dstSet = out_descriptor_set,
    //     .dstBinding = 0,
    //     .descriptorCount = 1,
    //     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    //     .pImageInfo = &image_info,
    // };

    // vkUpdateDescriptorSets(device, 1, &image_write, 0, nullptr);
}

void Graphics::Impl::init_vulkan_create_pipelines()
{}

void Graphics::Impl::start_new_frame()
{   // Increment frame counter for new frame.
    current_frame_idx++;

    // Wait until GPU has finished rendering last frame (of current frame index).
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };

    constexpr uint64_t k_10sec_as_ns{ 10'000'000'000 };

    VkResult err;
    err = vkWaitForFences(gfx.device, 1, &current_frame.render_fence, true, k_10sec_as_ns);
    if (err)
    {
        throw std::runtime_error("wait for render fence timed out.");
    }

    err = vkResetFences(gfx.device, 1, &current_frame.render_fence);
    if (err)
    {
        throw std::runtime_error("reset render fence failed.");
    }

    // Request image from swapchain.
    err = vkAcquireNextImageKHR(gfx.device,
                                gfx.swapchain,
                                k_10sec_as_ns,
                                current_frame.swapchain_semaphore,
                                nullptr,
                                &current_swapchain_image_idx);
    if (err)
    {
        throw std::runtime_error("Acquire next swapchain image failed.");
    }

    // Reset command buffers.
    current_frame.graphics_queue_command_buffer.reset();
}

void Graphics::Impl::render_incomplete_jojojojojojs()
{
    // @INCOMPLETE: just to get the screen to show.
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto& swapchain_img{ gfx.swapchain_images[current_swapchain_image_idx] };

    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    swapchain_img.transition_to(cmd, VK_IMAGE_LAYOUT_GENERAL);

	// Clear image.
    VkClearColorValue clear_value;
	float_t flash = std::abs(std::sin(current_frame_idx / 120.f));
	clear_value = { { 0.0f, 0.0f, flash, 1.0f } };
    VkImageSubresourceRange clear_range = txp_vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    assert(swapchain_img.get_layout() == VK_IMAGE_LAYOUT_GENERAL);
    vkCmdClearColorImage(cmd,
                         swapchain_img.get(),
                         swapchain_img.get_layout(),
                         &clear_value,
                         1,
                         &clear_range);

    swapchain_img.transition_to(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void Graphics::Impl::present_frame_to_screen()
{
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };

    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // End recording command buffers.
    current_frame.graphics_queue_command_buffer.finish();



    //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished
    VkResult err;

    VkCommandBufferSubmitInfo cmd_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0,
    };

    VkSemaphoreSubmitInfo signal_info =
        txp_vk_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                     current_frame.render_semaphore);
    VkSemaphoreSubmitInfo wait_info =
        txp_vk_semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                     current_frame.swapchain_semaphore);

    VkSubmitInfo2 submit = txp_vk_submit_info(&cmd_info, &signal_info, &wait_info);
    

    //submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	err = vkQueueSubmit2(gfx.graphics_queue, 1, &submit, current_frame.render_fence);
    if (err)
    {
        throw std::runtime_error("Queue submit failed.");
    }



    //prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.pSwapchains = &gfx.swapchain;
	present_info.swapchainCount = 1;

	present_info.pWaitSemaphores = &current_frame.render_semaphore;
	present_info.waitSemaphoreCount = 1;

	present_info.pImageIndices = &current_swapchain_image_idx;

	err = vkQueuePresentKHR(gfx.graphics_queue, &present_info);
    if (err)
    {
        throw std::runtime_error("Queue present KHR failed.");
    }
}


// class Graphics
TXP::Graphics::Graphics(std::string const& title, int32_t width, int32_t height)
    : m_pimpl(std::make_unique<Impl>(title, width, height))
{
    m_pimpl->init_glfw_no_api();
    m_pimpl->init_window_props();
    m_pimpl->init_window();
    m_pimpl->init_vulkan_instance();
    m_pimpl->init_vulkan_window_surface();
    m_pimpl->init_vulkan_build_device();
    m_pimpl->init_vulkan_create_memory_allocator();
    m_pimpl->init_vulkan_build_swapchain();
    m_pimpl->init_vulkan_retrieve_queues();
    m_pimpl->init_vulkan_create_cmd_structures();
    m_pimpl->init_vulkan_create_sync_structures();
    m_pimpl->init_vulkan_allocate_descriptors();
    m_pimpl->init_vulkan_create_pipelines();
}

TXP::Graphics::~Graphics()
{
    // @TODO
    assert(false);
}

void TXP::Graphics::start_new_frame()
{
    m_pimpl->start_new_frame();
}

void TXP::Graphics::compute_light_culling()
{
    assert(false);
}

void TXP::Graphics::compute_shadow_culling()
{
    assert(false);
}

void TXP::Graphics::compute_opaque_geometry_culling()
{
    assert(false);
}

void TXP::Graphics::compute_transparent_geometry_culling()
{
    assert(false);
}

void TXP::Graphics::render_shadows()
{
    assert(false);
}

void TXP::Graphics::render_opaque_geometry()
{
    assert(false);
}

void TXP::Graphics::render_clouds()
{
    assert(false);
}

void TXP::Graphics::render_volumetric_light()
{
    assert(false);
}

void TXP::Graphics::render_particles()
{
    assert(false);
}

void TXP::Graphics::render_transparent_geometry()
{
    assert(false);
}

void TXP::Graphics::render_hdr_to_ldr_postprocessing()
{
    // @INCOMPLETE: just to get the screen to show.
    m_pimpl->render_incomplete_jojojojojojs();
}

void TXP::Graphics::render_imgui()
{
    assert(false);
}

void TXP::Graphics::present_frame_to_screen()
{
    m_pimpl->present_frame_to_screen();
}

}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
