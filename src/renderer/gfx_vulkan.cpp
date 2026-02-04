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
#include <stdexcept>
#include <string>


namespace
{

void init_glfw_no_api()
{
    auto result = glfwInit();
    assert(result == GLFW_TRUE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void apply_window_props(int32_t win_width, int32_t win_height)
{
    assert(win_width > 0 && win_height > 0);

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
            static_cast<int32_t>(monitor_workarea.width * 0.5 - win_width * 0.5),
        monitor_workarea.ypos +
            static_cast<int32_t>(monitor_workarea.height * 0.5 - win_height * 0.5),
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

GLFWwindow* create_window(std::string const& title, int32_t win_width, int32_t win_height)
{
    assert(win_width > 0 && win_height > 0);

    auto glfw_window = glfwCreateWindow(win_width, win_height, title.c_str(), nullptr, nullptr);
    if (!glfw_window)
    {
        glfwTerminate();
        throw std::runtime_error("Window creation failed.");
    }

    // @TODO: add window icon here.


    glfwShowWindow(glfw_window);

    // // Window callbacks.
    // // @NOTE: With key callbacks etc that's also used by Imgui, Imgui
    // //   chains these callbacks so they don't get lost.
    // glfwSetKeyCallback(m_window, key_callback);
    // glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    // glfwSetCursorPosCallback(m_window, cursor_position_callback);
    // glfwSetScrollCallback(m_window, scroll_callback);
    // glfwSetWindowFocusCallback(m_window, window_focus_callback);
    // glfwSetWindowIconifyCallback(m_window, window_iconify_callback);

    return glfw_window;
}


/// Holds Vulkan graphics initialization information.
struct Vk_gfx_instance
{
    vkb::Instance vkb_instance;
    vkb::Device vkb_device;

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
    std::vector<VkImage> swapchain_images;
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
static Vk_gfx_instance s_gfx;


/// Number of frames-in-flight.
constexpr uint32_t k_frame_overlap{ 3 };

/// Holds per-frame data.
struct Frame_data
{
    VkCommandPool command_pool;
    VkCommandBuffer graphics_queue_command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;

    // @TODO: figure out the vv below vv
    // vk_buffer::Allocated_buffer camera_buffer;
    // vk_buffer::GPU_geo_per_frame_buffer geo_per_frame_buffer;
};
static std::array<Frame_data, k_frame_overlap> s_frames;


void init_vulkan_instance()
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

    s_gfx.vkb_instance = instance_build_result.value();
    s_gfx.instance = s_gfx.vkb_instance.instance;
#ifndef NDEBUG
    s_gfx.debug_utils_messenger = s_gfx.vkb_instance.debug_messenger;
#endif
}

void init_vulkan_window_surface(GLFWwindow* window)
{
    VkResult err = glfwCreateWindowSurface(s_gfx.instance, window, nullptr, &s_gfx.surface);
    if (err)
    {
        throw std::runtime_error("Vulkan surface creation failed.");
    }
}

void init_vulkan_build_device()
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
        // For `vkCmdDrawIndexedIndirectCount`.
        .drawIndirectCount = VK_TRUE,
        // For non-uniform, dynamic arrays of textures in shaders.
        .descriptorIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        // For MIN/MAX sampler when creating mip chains for occlusion culling.
        .samplerFilterMinmax = VK_TRUE,
        // For buffer references in the stead of descriptor sets.
        .bufferDeviceAddress = VK_TRUE,
    };

    // Select physical device.
    vkb::PhysicalDeviceSelector selector{ s_gfx.vkb_instance };
    vkb::PhysicalDevice physical_device{
        selector
            .set_minimum_version(1, 3)
            .set_required_features_13(vulkan13_features)
            .set_required_features_12(vulkan12_features)
            .set_surface(s_gfx.surface)
            .set_required_features({
                // @NOTE: @FEATURES: Enable required features right here
                // .multiDrawIndirect = VK_TRUE,         // So that vkCmdDrawIndexedIndirect() can be called with a >1 drawCount. (@NOTE: not happening with current setup)
                .depthClamp = VK_TRUE,                // For shadow maps, this is really nice.
                .fillModeNonSolid = VK_TRUE,          // To render wireframes.
                .samplerAnisotropy = VK_TRUE,
                .fragmentStoresAndAtomics = VK_TRUE,  // For the picking buffer! @TODO: If a release build then disable.
            })
            .select()
            .value()
    };
    s_gfx.physical_device = physical_device.physical_device;
    s_gfx.physical_device_properties = physical_device.properties;

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
    s_gfx.vkb_device =
        device_builder
            .add_pNext(&shader_draw_parameters_features)
            .build()
            .value();
    s_gfx.device = s_gfx.vkb_device.device;
}

void init_vulkan_create_memory_allocator()
{   // Initialize VMA.
    VmaAllocatorCreateInfo vma_allocator_info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,  // To access GPU pointers.
        .physicalDevice = s_gfx.physical_device,
        .device = s_gfx.device,
        .instance = s_gfx.instance,
    };
    vmaCreateAllocator(&vma_allocator_info, &s_gfx.allocator);
}

void init_vulkan_build_swapchain(int32_t window_width, int32_t window_height)
{   // Build swapchain.
    vkb::SwapchainBuilder swapchain_builder{ s_gfx.physical_device, s_gfx.device, s_gfx.surface };
    vkb::Swapchain swapchain{
        swapchain_builder.use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // Mailbox (G-Sync/Freesync).
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)    // FIFO (V-Sync).
            .set_desired_extent(window_width, window_height)
            // @TODO: TRANSFER_DST image usage added below. Try removing once renderer is finished
            // (assuming you're not gonna have some kind of image transfer as the last step into the
            // swapchain image).
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value()
    };

    s_gfx.swapchain = swapchain.swapchain;
    s_gfx.swapchain_images = swapchain.get_images().value();
    s_gfx.swapchain_image_views = swapchain.get_image_views().value();
    s_gfx.swapchain_image_format = swapchain.image_format;
    s_gfx.swapchain_extent.width = window_width;
    s_gfx.swapchain_extent.height = window_height;
}

void init_vulkan_retrieve_queues()
{
    s_gfx.graphics_queue = s_gfx.vkb_device.get_queue(vkb::QueueType::graphics).value();
    s_gfx.graphics_queue_family_idx = s_gfx.vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // @NOTE: the vv below vv queues are left unused currently.

    s_gfx.async_compute_queue = s_gfx.vkb_device.get_queue(vkb::QueueType::compute).value();
    s_gfx.async_compute_queue_family_idx = s_gfx.vkb_device.get_queue_index(vkb::QueueType::compute).value();

    s_gfx.transfer_queue = s_gfx.vkb_device.get_queue(vkb::QueueType::transfer).value();
    s_gfx.transfer_queue_family_idx = s_gfx.vkb_device.get_queue_index(vkb::QueueType::transfer).value();
}

void init_vulkan_create_cmd_structures()
{
    VkResult err;

    VkCommandPoolCreateInfo cmd_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = s_gfx.graphics_queue_family_idx,
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateCommandPool(s_gfx.device, &cmd_pool_info, nullptr, &s_frames[i].command_pool);
        if (err)
        {
            throw std::runtime_error("Vulkan command pool creation failed for frame #");
        }

        // Allocate default cmd buffer for rendering.
        VkCommandBufferAllocateInfo cmd_alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = s_frames[i].command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        err = vkAllocateCommandBuffers(s_gfx.device, &cmd_alloc_info, &s_frames[i].graphics_queue_command_buffer);
        if (err)
        {
            throw std::runtime_error("Vulkan command pool allocation failed for frame #");
        }
    }
}

void init_vulkan_create_sync_structures()
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
        err = vkCreateFence(s_gfx.device, &fence_create_info, nullptr, &s_frames[i].render_fence);
        if (err)
        {
            throw std::runtime_error("Vulkan render fence creation failed for frame #");
        }

        err = vkCreateSemaphore(s_gfx.device, &semaphore_create_info, nullptr, &s_frames[i].swapchain_semaphore);
        if (err)
        {
            throw std::runtime_error("Vulkan swapchain semaphore creation failed for frame #");
        }

        err = vkCreateSemaphore(s_gfx.device, &semaphore_create_info, nullptr, &s_frames[i].render_semaphore);
        if (err)
        {
            throw std::runtime_error("Vulkan render semaphore creation failed for frame #");
        }
    }
}

void init_vulkan_allocate_descriptors()
{
    // Init allocator pool.
    std::vector<vk_desc::Descriptor_allocator::Pool_size_ratio> sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };

    out_descriptor_alloc.init_pool(device, 10, sizes);

    // Build layout.
    vk_desc::Descriptor_layout_builder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    out_descriptor_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

    // Allocate descriptor set.
    out_descriptor_set = out_descriptor_alloc.allocate(device, out_descriptor_layout);

    VkDescriptorImageInfo image_info{
        .imageView = hdr_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet image_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = out_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(device, 1, &image_write, 0, nullptr);
}

void init_vulkan_create_pipelines()
{
    assert(false);
}

}  // namespace


// TXP::GFX
void TXP::GFX::setup_renderer(std::string const& title, int32_t width, int32_t height)
{
    init_glfw_no_api();
    apply_window_props(width, height);
    auto window = create_window(title, width, height);
    init_vulkan_instance();
    init_vulkan_window_surface(window);
    init_vulkan_build_device();
    init_vulkan_create_memory_allocator();
    init_vulkan_build_swapchain(width, height);
    init_vulkan_retrieve_queues();
    init_vulkan_create_cmd_structures();
    init_vulkan_create_sync_structures();
    init_vulkan_allocate_descriptors();
    init_vulkan_create_pipelines();
}

void TXP::GFX::teardown_renderer()
{
    // @TODO
    assert(false);
}

uint32_t TXP::GFX::acquire_next_image()
{
    VkResult err;

    // Wait until GPU has finished rendering last frame.
    constexpr uint64_t k_10sec_as_ns{ 10'000'000'000 };

    err = vkWaitForFences(device, 1, &current_frame.render_fence, true, k_10sec_as_ns);
    if (err)
    {
        throw std::runtime_error("wait for render fence timed out.");
    }

    err = vkResetFences(device, 1, &current_frame.render_fence);
    if (err)
    {
        throw std::runtime_error("reset render fence failed.");
    }

    // Request image from swapchain.
    err = vkAcquireNextImageKHR(device,
                                swapchain,
                                k_10sec_as_ns,
                                current_frame.swapchain_semaphore,
                                nullptr,
                                &out_swapchain_image_idx);
    if (err)
    {
        throw std::runtime_error("Acquire next swapchain image failed.");
    }
}

#endif // TXP_GFX_BACKEND_VULKAN
