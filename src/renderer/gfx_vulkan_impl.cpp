#if TXP_GFX_BACKEND_VULKAN

#include "gfx_vulkan_impl.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#   define VMA_IMPLEMENTATION
#   include <vk_mem_alloc.h>
#pragma clang diagnostic pop

#include <GLFW/glfw3.h>
#include "VkBootstrap.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#define KHRONOS_STATIC 1
#include "ktx.h"
#include "ktxvulkan.h"
// clang-format on

#include "btlogger.h"
#include "btservice_finder.h"
#include "editor_conent/editor_content.h"
#include "gfx_vulkan/vk_image.h"
#include "gfx_vulkan/vk_structs.h"
#include "input_handler/input_handler.h"
#include "input_handler/input_key_codes.h"
#include "render_object/render_model.h"
#include "render_object/vertex.h"
#include "renderer/gfx.h"
#include "renderer/types.h"
#include "shader_creation/shader_creation.h"
#include "txp_renderer/renderer.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


namespace
{

// Helper pointers for GLFW callbacks.
static TXP::Input::Input_handler* s_input_handler{ nullptr };

// GLFW window callbacks.
static void key_callback(GLFWwindow* window,
                         int32_t key,
                         int32_t scancode,
                         int32_t action,
                         int32_t mods)
{
    s_input_handler->keyboard_event(key,
                                    action == GLFW_PRESS || action == GLFW_REPEAT,
                                    action == GLFW_REPEAT,
                                    mods);
}

static void mouse_button_callback(GLFWwindow* window,
                                  int32_t button,
                                  int32_t action,
                                  int32_t mods)
{
    s_input_handler->mouse_button_event(button, action == GLFW_PRESS, mods);
}

static void cursor_position_callback(GLFWwindow* window,
                                     double_t xpos,
                                     double_t ypos)
{
    s_input_handler->cursor_position_event(xpos, ypos);
}

static void scroll_callback(GLFWwindow* window,
                            double_t xoffset,
                            double_t yoffset)
{
    s_input_handler->scroll_event(xoffset, yoffset);
}

static void joystick_callback(int32_t jid, int32_t event)
{
    s_input_handler->gamepad_connect_event(jid, event == GLFW_CONNECTED);
}

static void window_focus_callback(GLFWwindow* window,
                                  int32_t focused)
{
    s_input_handler->window_focus_event(focused == GLFW_TRUE);
}

static void window_iconify_callback(GLFWwindow* window,
                                    int32_t iconified)
{
    s_input_handler->window_iconify_event(iconified == GLFW_TRUE);
}

static void window_resize_callback(GLFWwindow* window,
                                   int32_t width,
                                   int32_t height)
{
    s_input_handler->window_resize_event(width, height);
}

static void window_close_callback(GLFWwindow* window)
{
    BT::service_finder::find_service<TXP::Renderer>().shutdown_loop();
}

}  // namespace


namespace TXP
{

void Graphics::Impl::init_glfw_no_api()
{
    auto result = glfwInit();
    assert(result == GLFW_TRUE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Graphics::Impl::init_window_props()
{
    assert(window_dims[0] > 0 && window_dims[1] > 0);

    auto target_monitor{ glfwGetPrimaryMonitor() };

    // Apply centering hints.
    struct Monitor_workarea
    {
        int32_t xpos;
        int32_t ypos;
        int32_t width;
        int32_t height;
    } monitor_workarea;
    glfwGetMonitorWorkarea(target_monitor,
                           &monitor_workarea.xpos,
                           &monitor_workarea.ypos,
                           &monitor_workarea.width,
                           &monitor_workarea.height);

    int32_t centered_window_pos[2]{
        monitor_workarea.xpos +
            static_cast<int32_t>(monitor_workarea.width * 0.5 - window_dims[0] * 0.5),
        monitor_workarea.ypos +
            static_cast<int32_t>(monitor_workarea.height * 0.5 - window_dims[1] * 0.5),
    };

    glfwWindowHint(GLFW_POSITION_X, centered_window_pos[0]);
    glfwWindowHint(GLFW_POSITION_Y, centered_window_pos[1]);

    // Get monitor scaling.
    monitor_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(target_monitor);

    // @TODO: implement vv below vv
    // glfwWindowHint(GLFW_RESIZABLE, app_window_settings.is_resizable ? GLFW_TRUE : GLFW_FALSE);
    // glfwWindowHint(GLFW_DECORATED, app_window_settings.has_border ? GLFW_TRUE : GLFW_FALSE);
    // glfwWindowHint(GLFW_MAXIMIZED, app_window_settings.is_maximized ? GLFW_TRUE : GLFW_FALSE);

    // submit_window_dims(win_dims.width, win_dims.height);  // @TODO

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
}

void Graphics::Impl::init_window()
{
    assert(window_dims[0] > 0 && window_dims[1] > 0);
    assert(window == nullptr);

    window = glfwCreateWindow(window_dims[0],
                              window_dims[1],
                              window_title.c_str(),
                              nullptr,
                              nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Window creation failed.");
    }

    // @TODO: add window icon here.


    // Window callbacks.
    // @NOTE: With key callbacks etc that's also used by Imgui, Imgui
    //   chains these callbacks so they don't get lost.
    s_input_handler = &BT::service_finder::find_service<Input::Input_handler>();
    if (s_input_handler == nullptr)
        throw std::runtime_error("No Input_handler service found.");

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetJoystickCallback(joystick_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);
    glfwSetWindowCloseCallback(window, window_close_callback);

    // Finish.
    glfwShowWindow(window);
}


Graphics::Impl::Frame_data& Graphics::Impl::get_current_frame()
{
    return frames[current_frame_idx % k_frame_overlap];
}

uint32_t Graphics::Impl::get_current_frame_idx()
{
    return (current_frame_idx % k_frame_overlap);
}

Vk_Image::Image& Graphics::Impl::get_current_swapchain_image()
{
    return gfx.swapchain_images[current_swapchain_image_idx];
}

VkImageView Graphics::Impl::get_current_swapchain_image_view()
{
    return gfx.swapchain_image_views[current_swapchain_image_idx];
}

VkSemaphore Graphics::Impl::get_current_swapchain_submit_semaphore()
{
    return gfx.swapchain_submit_semaphores[current_swapchain_image_idx];
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
        // For `rgba16f` format.
        .shaderFloat16 = VK_TRUE,
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

void Graphics::Impl::select_vulkan_window_surface_format()
{   // Check for WSI support.
    // VkBool32 res;
    // vkGetPhysicalDeviceSurfaceSupportKHR(gfx.physical_device, gfx.graphics_queue_family_idx, gfx.surface, &res);
    // if (res != VK_TRUE)
    // {
    //     throw std::runtime_error("Error no WSI support on physical device.");
    // }

    // Select surface format.
    constexpr VkFormat request_surface_image_formats[]{ VK_FORMAT_B8G8R8A8_UNORM,
                                                        VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_FORMAT_B8G8R8_UNORM,
                                                        VK_FORMAT_R8G8B8_UNORM };
    constexpr VkColorSpaceKHR request_surface_color_space{ VK_COLORSPACE_SRGB_NONLINEAR_KHR };

    VkPhysicalDeviceSurfaceInfo2KHR surface_info{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .pNext = nullptr,
        .surface = gfx.surface,
    };
    uint32_t avail_cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gfx.physical_device, gfx.surface, &avail_cnt, nullptr);

    std::vector<VkSurfaceFormatKHR> avail_formats;
    avail_formats.resize(avail_cnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gfx.physical_device,
                                         gfx.surface,
                                         &avail_cnt,
                                         avail_formats.data());

    bool found{ false };
    for (auto const& avail_format : avail_formats)
    {
        for (auto request_format : request_surface_image_formats)
        {
            if (avail_format.format == request_format &&
                avail_format.colorSpace == request_surface_color_space)
            {
                gfx.surface_format.format = avail_format.format;
                gfx.surface_format.colorSpace = avail_format.colorSpace;
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        gfx.surface_format.format = avail_formats.front().format;
        gfx.surface_format.colorSpace = avail_formats.front().colorSpace;
    }
}

void Graphics::Impl::init_vulkan_build_swapchain()
{   // Get framebuffer size (if different from window size).
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);

    // Build swapchain.
    vkb::SwapchainBuilder swapchain_builder{ gfx.physical_device, gfx.device, gfx.surface };
    vkb::Swapchain swapchain{
        swapchain_builder
            .set_desired_format(gfx.surface_format)
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // G-Sync.
            .add_fallback_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)  // Freesync / V-Sync off.
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)    // V-Sync on.
            .set_desired_extent(fb_width, fb_height)
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

        std::vector<Vk_Image::Image> images;
        images.reserve(swapchain_imgs_val.size());

        for (auto img : swapchain_imgs_val)
            images.emplace_back(img, VK_IMAGE_ASPECT_COLOR_BIT);
        
        return images;
    })();

    gfx.swapchain_image_views = swapchain.get_image_views().value();
    gfx.swapchain_submit_semaphores.resize(gfx.swapchain_images.size());
    gfx.swapchain_image_format = swapchain.image_format;
    gfx.swapchain_extent.width = fb_width;
    gfx.swapchain_extent.height = fb_height;
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

    for (auto& sss : gfx.swapchain_submit_semaphores)
    {
        err = vkCreateSemaphore(gfx.device,
                                &semaphore_create_info,
                                nullptr,
                                &sss);
        if (err)
        {
            throw std::runtime_error("Vulkan swapchain submit semaphore creation failed.");
        }
    }

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateFence(gfx.device, &fence_create_info, nullptr, &frames[i].render_fence);
        if (err)
        {
            throw std::runtime_error("Vulkan render fence creation failed.");
        }

        err = vkCreateSemaphore(gfx.device,
                                &semaphore_create_info,
                                nullptr,
                                &frames[i].acquire_nxt_img_semaphore);
        if (err)
        {
            throw std::runtime_error("Vulkan acquire next img semaphore creation failed.");
        }
    }
}

void Graphics::Impl::init_vulkan_for_imgui()
{
    {   // Create descriptor pool.
        VkDescriptorPoolSize pool_sizes[]{
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        VkResult err;
        err = vkCreateDescriptorPool(gfx.device,
                                     &pool_info,
                                     nullptr,
                                     &gfx.imgui_desc_pool);
        if (err)
        {
            throw std::runtime_error("Creating ImGui descriptor pool failed.");
        }
    }

    // Setup dear ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable gamepad controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking

    // @NOTE: disabled viewports due to performance loss and incorrect swapchain timing in current
    //        ImGui code.  -Thea 2026/02/14
    // @AMEND: also, on Linux, only X11 is supported for viewports. Wayland is not. So it's disabled
    //         in the actual code for this functionality in wayland linux.  -Thea 2026/02/15
    #if 0
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport / platform windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    #endif // 0

    // Setup dear ImGui style.
    ImGui::StyleColorsDark();

    // Setup scaling.
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(monitor_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = monitor_scale;  // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
    io.ConfigDpiScaleFonts = true;       // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;   // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    // When viewports are enabled make window corners sharp and opaque.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup platform/renderer backends.
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = gfx.instance;
    init_info.PhysicalDevice = gfx.physical_device;
    init_info.Device = gfx.device;
    init_info.QueueFamily = gfx.graphics_queue_family_idx;
    init_info.Queue = gfx.graphics_queue;
    init_info.PipelineCache = gfx.pipeline_cache;
    init_info.DescriptorPool = gfx.imgui_desc_pool;
    init_info.MinImageCount = static_cast<uint32_t>(gfx.swapchain_images.size());
    init_info.ImageCount = static_cast<uint32_t>(gfx.swapchain_images.size());
    init_info.Allocator = nullptr;
    init_info.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfo pipe_rend_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &gfx.swapchain_image_format,
    };
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipe_rend_create_info;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

void Graphics::Impl::init_vulkan_render_graph_resources()
{
    Vk_Image::Allocated_image::set_vk_props(gfx.physical_device, gfx.device, gfx.allocator);

    std::vector<Render_view_size> const default_rend_view_sizes{
        Render_view_size{ .width = 1280, .height = 720 }
    };

    set_render_view_sizes(default_rend_view_sizes);

    // Create model transform set buffer.
    for (auto& frame : frames)
    {
        frame.per_instance_data_collection_buffer.create(
            gfx.device,
            gfx.allocator,
            sizeof(gpu_type::Per_instance_data_collection),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
        frame.model_transform_set_buffer.create(
            gfx.device,
            gfx.allocator,
            sizeof(gpu_type::Model_transform_set),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }
}

void Graphics::Impl::init_vulkan_create_descriptors()
{
    // @TODO: @THEA: abstract this into the reflection-based version.

    std::vector<Descriptor_allocator::Pool_size_ratio> sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
    };

    global_descriptor_allocator.init_pool(gfx.device, gfx.allocator, 10, std::move(sizes));
}

void Graphics::Impl::construct_ktx_vk_device_info()
{
    // ktxVulkanDeviceInfo kvdi{
    //     .instance = gfx.instance,
    //     .physicalDevice = gfx.physical_device,
    //     .device = gfx.device,
    //     .queue = gfx.graphics_queue,  // @TODO: maybe add to transfer queue? This should be good enough for now tho.  -Thea 2026/02/08
    //     .cmdBuffer = frames.front().graphics_queue_command_buffer.get(),
    //     .cmdPool = frames.front().command_pool,
    // };
    ktxVulkanDeviceInfo_Construct(&ktx_vk_device_info,
                                  gfx.physical_device,
                                  gfx.device,
                                  gfx.graphics_queue,
                                  frames.front().command_pool,  // Use first available command pool.
                                  nullptr);
}

void Graphics::Impl::destruct_ktx_vk_device_info()
{
    ktxVulkanDeviceInfo_Destruct(&ktx_vk_device_info);
}

ktxVulkanTexture Graphics::Impl::load_and_upload_texture(std::string const& fname)
{   // Load from file.
    ktxTexture* ktxtexture;
    KTX_error_code ktxresult;

    ktxresult = ktxTexture_CreateFromNamedFile(fname.c_str(),
                                               KTX_TEXTURE_CREATE_NO_FLAGS,
                                               &ktxtexture);
    if (ktxresult != KTX_SUCCESS)
    {
        std::stringstream ss;
        ss << "Creation of ktxTexture from \"" << fname
           << "\" failed: " << ktxErrorString(ktxresult) << "\n"
           << "  errno: " << errno;
        throw std::runtime_error(ss.str());
    }

    // Upload to GPU.
    ktxVulkanTexture ktx_vk_texture;

    ktxresult = ktxTexture_VkUpload(ktxtexture, &ktx_vk_device_info, &ktx_vk_texture);
    if (ktxresult != KTX_SUCCESS)
    {
        std::stringstream ss;
        ss << "ktxTexture_VkUploadEx() failed: " << ktxErrorString(ktxresult);
        throw std::runtime_error(ss.str());
    }

    // Find orientation of ST texcoords.
    int32_t texcoords_s_sign{ 1 };
    int32_t texcoords_t_sign{ 1 };

    char* p_val;
    uint32_t val_len;
    if (KTX_SUCCESS == ktxHashList_FindValue(&ktxtexture->kvDataHead,
                                             KTX_ORIENTATION_KEY,
                                             &val_len,
                                             (void**)&p_val))
    {
        char s, t;
        if (sscanf(p_val, KTX_ORIENTATION2_FMT, &s, &t) == 2)
        {
            if (s == 'l') texcoords_s_sign = -1;
            if (t == 'u') texcoords_t_sign = -1;
        }
    }

    // Cleanup.
    ktxTexture_Destroy(ktxtexture);

    // Finish.
    return ktx_vk_texture;
}

void Graphics::Impl::add_texture_entry(std::string const& texture_name,
                                       ktxVulkanTexture&& allocated_image)
{
    texture_entries.emplace(texture_name, std::move(allocated_image));
}


void Graphics::Impl::upload_model_entries_to_gpu(
    Render_model_data_collection& data_collection)
{   // Get static model information.
    std::vector<std::string> static_model_names =
        data_collection.get_static_model_data_set_name_list();

    std::vector<Static_model_data_set*> static_models;
    static_models.reserve(static_model_names.size());

    for (auto const& name : static_model_names)
        static_models.emplace_back(
            const_cast<Static_model_data_set*>(&data_collection.get_static_model_data_set(
                data_collection.get_static_model_data_set_idx(name))));

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Transform to singular static model.

    // Reserve space for vertices and indices.
    size_t vertex_count{ 0 };
    size_t index_count{ 0 };

    for (auto model : static_models)
    {
        // @NOTE: using `vertex_count` for offset since that is the base of the vertex index.
        model->vertex_index_offset = vertex_count;

        vertex_count += model->vertices.size();

        for (auto& mesh : model->meshes)
        {   // Mark where to start in index buffer for this mesh.
            model->first_index_offsets.emplace_back(index_count);

            index_count += mesh.indices.size();
        }
    }

    // Copy vertices and indices.
    std::vector<Vertex> combined_vertices(vertex_count);
    std::vector<uint32_t> combined_indices(index_count);

    vertex_count = 0;
    index_count = 0;

    for (auto model : static_models)
    {
        std::memcpy(combined_vertices.data() + vertex_count,
                    model->vertices.data(),
                    model->vertices.size() * sizeof(Vertex));
        vertex_count += model->vertices.size();

        for (auto const& mesh : model->meshes)
        {
            std::memcpy(combined_indices.data() + index_count,
                        mesh.indices.data(),
                        mesh.indices.size() * sizeof(uint32_t));
            index_count += mesh.indices.size();
        }
    }

    // Upload to GPU.
    // @REF: using method from "https://howtovulkan.com/#loading-meshes" instead of staging buffers.
    //         -Thea 2026/02/21
    VkDeviceSize vertex_buf_size{ sizeof(Vertex) * combined_vertices.size() };
    VkDeviceSize index_buf_size{ sizeof(uint32_t) * combined_indices.size() };

    combined_static_model.vertex_index_buffer.create(
        gfx.device,
        gfx.allocator,
        vertex_buf_size + index_buf_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    combined_static_model.offset_to_index_buffer = vertex_buf_size;

    void* p_mapped_data{ combined_static_model.vertex_index_buffer.get_p_mapped_data() };
    auto offset_to_idx_buf{ combined_static_model.offset_to_index_buffer };

    std::memcpy(p_mapped_data,
                combined_vertices.data(),
                vertex_buf_size);
    std::memcpy(reinterpret_cast<char*>(p_mapped_data) + offset_to_idx_buf,
                combined_indices.data(),
                index_buf_size);
}

VkDescriptorSetLayout Graphics::Impl::build_descriptor_layout(
    Descriptor_binding_set_t&& bindings,
    VkShaderStageFlags shader_stages,
    VkDescriptorSetLayoutCreateFlags flags)
{
    assert(gfx.device);
    assert(gfx.allocator);

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
    layout_bindings.reserve(bindings.size());

    // Optional: variable descriptor count binding.
    uint32_t num_variable_descriptor_count_bindings{ 0 };
    VkDescriptorBindingFlags desc_variable_flag;
    VkDescriptorSetLayoutBindingFlagsCreateInfo desc_binding_flags_info;

    for (auto [bind_idx, descriptor_type] : bindings)
    {
        layout_bindings.emplace_back(VkDescriptorSetLayoutBinding{
            .binding = bind_idx,
            .descriptorType = descriptor_type.descriptor_type,
            .descriptorCount = (!descriptor_type.use_variable_descriptor_count_binding_flag
                                    ? 1
                                    : descriptor_type.variable_descriptor_count),
            .stageFlags = shader_stages,
        });

        // Optional: variable descriptor count binding.
        if (descriptor_type.use_variable_descriptor_count_binding_flag)
        {
            desc_variable_flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            desc_binding_flags_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = &desc_variable_flag
            };

            num_variable_descriptor_count_bindings++;
        }
    }

    // Failsafe.
    if (num_variable_descriptor_count_bindings > 1)
    {
        BT_ERRORF(
            "Unable to support more than 1 variable descriptor count binding. Num registered: %u",
            num_variable_descriptor_count_bindings);
        throw std::runtime_error("Unsupported operation. Add support for this.");  // @THEA: also I don't know how to support multiple of these lol.
    }

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = (num_variable_descriptor_count_bindings == 0 ? nullptr : &desc_binding_flags_info),
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
        .pBindings = layout_bindings.data(),
    };

    // Create descriptor set layout.
    VkDescriptorSetLayout layout;
    VkResult err = vkCreateDescriptorSetLayout(gfx.device,
                                               &layout_info,
                                               nullptr,
                                               &layout);

    if (err)
        throw std::runtime_error("Creating descriptor set layout failed.");

    return layout;
}


void Graphics::Impl::poll_input_events()
{
    glfwPollEvents();
}

void Graphics::Impl::build_imgui_contents(std::vector<Render_view_size>& out_rend_view_sizes)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    editor_content::Render_view_image_content render_view_image_content;
    render_view_image_content.content_image_descriptors.reserve(render_views.size());
    for (auto const& rv : render_views)
    {
        render_view_image_content.content_image_descriptors.emplace_back(
            rv.imgui_color_image_descriptor);
    }

    editor_content::build_content(*s_input_handler, render_view_image_content, out_rend_view_sizes);

    // Convert to render instructions.
    ImGui::Render();
}

void Graphics::Impl::set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes)
{
    bool is_gpu_idle{ false };  // Wait until GPU idle if any mutation.
    auto ensure_gpu_idle_fn = [this, &is_gpu_idle]() {
        if (!is_gpu_idle)
        {
            wait_until_gpu_idle();
            is_gpu_idle = true;
        }
    };

    if (render_view_imgui_image_sampler == VK_NULL_HANDLE)
    {   // Create image sampler for render views.
        VkSamplerCreateInfo sampler_info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .minLod = -1000,
            .maxLod = 1000,
        };
        VkResult err =
            vkCreateSampler(gfx.device, &sampler_info, nullptr, &render_view_imgui_image_sampler);
        if (err)
            throw std::runtime_error("Couldn't create the stupid little sampler. Wtf.");
    }

    size_t prev_render_views_size{ render_views.size() };

    auto& current_frame{ get_current_frame() };
    size_t prev_env_data_buffers_size{ current_frame.environment_data_buffers.size() };

    // Destroy render views if shrinking its list's size.
    for (size_t i = rend_view_sizes.size(); i < prev_render_views_size; i++)
    {
        ensure_gpu_idle_fn();

        auto& render_view{ render_views[i] };
        render_view.color_image.teardown();
        render_view.depth_image.teardown();
        ImGui_ImplVulkan_RemoveTexture(render_view.imgui_color_image_descriptor);
    }

    // Destroy environment data buffers if shrinking its list's size.
    for (size_t i = rend_view_sizes.size(); i < prev_env_data_buffers_size; i++)
    {
        ensure_gpu_idle_fn();
        current_frame.environment_data_buffers[i].destroy();
    }

    // Resize.
    render_views.resize(rend_view_sizes.size());
    current_frame.environment_data_buffers.resize(rend_view_sizes.size());

    // Create new/changed render views.
    for (size_t i = 0; i < render_views.size(); i++)
    {
        auto& render_view{ render_views[i] };
        auto const& rend_view_size{ rend_view_sizes[i] };

        if (rend_view_size.width <= 0 || rend_view_size.height <= 0)
            throw std::runtime_error("Invalid render view size.");

        if (i < prev_render_views_size)
        {
            if (rend_view_size.width == render_view.color_image.get_extent().width &&
                rend_view_size.height == render_view.color_image.get_extent().height)
                continue;  // Skip this image since nothing has changed.

            // Destroy image since this has changed.
            ensure_gpu_idle_fn();
            render_view.color_image.teardown();
            render_view.depth_image.teardown();
            ImGui_ImplVulkan_RemoveTexture(render_view.imgui_color_image_descriptor);
        }

        // HDR draw image.
        render_view.render_view_idx = i;

        VkExtent2D extent{
            .width = static_cast<uint32_t>(rend_view_size.width),
            .height = static_cast<uint32_t>(rend_view_size.height),
        };

        render_view.color_image = Vk_Image::Allocated_image::create_image_2d(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            extent,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT |  // For ImGui render view sampling.
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        render_view.depth_image = Vk_Image::Allocated_image::create_image_depth_buffer(extent);

        render_view.imgui_color_image_descriptor =
            ImGui_ImplVulkan_AddTexture(render_view_imgui_image_sampler,
                                        render_view.color_image.get_image_view(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if (render_views.empty())
        throw std::runtime_error("Render-view list must not be empty.");

    // Create new environment data buffers.
    for (size_t i = 0; i < current_frame.environment_data_buffers.size(); i++)
    {
        auto& env_data_buffer{ current_frame.environment_data_buffers[i] };

        if (i < prev_env_data_buffers_size)
            continue;  // Skip this buffer since nothing has changed.

        // Environment data buffers.
        env_data_buffer.create(gfx.device,
                               gfx.allocator,
                               sizeof(gpu_type::Environment_data),
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                   VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }
    if (current_frame.environment_data_buffers.empty())
        throw std::runtime_error("Environment data buffer list must not be empty.");
}

void Graphics::Impl::set_render_view_camera(size_t render_view_idx,
                                            mat4 camera_projection,
                                            mat4 camera_view)
{
    auto& env_data{ *static_cast<gpu_type::Environment_data*>(
        get_current_frame().environment_data_buffers[render_view_idx].get_p_mapped_data()) };

    glm_mat4_copy(camera_projection, env_data.projection);
    glm_mat4_copy(camera_view, env_data.view);
}

void Graphics::Impl::set_directional_light(size_t render_view_idx,
                                           vec3 direction,
                                           vec3 color,
                                           float_t intensity)
{
    auto& env_data{ *static_cast<gpu_type::Environment_data*>(
        get_current_frame().environment_data_buffers[render_view_idx].get_p_mapped_data()) };

    glm_vec3_copy(direction, env_data.directional_light.direction_xyz_intensity_w);
    env_data.directional_light.direction_xyz_intensity_w[3] = intensity;

    glm_vec3_copy(color, env_data.directional_light.color);

    // @NOCHECKIN: this is supposed to be a different func!
    env_data.lighting_mode = 1;
}

void Graphics::Impl::set_render_object_per_instance_data(
    std::vector<Render_object> const& rend_obj_list)
{
    size_t num_instances{ std::min(rend_obj_list.size(), static_cast<size_t>(65535)) };  // @HARDCODE: comes from shader.

    auto per_inst_data_ptr = static_cast<gpu_type::Per_instance_data*>(
        get_current_frame().per_instance_data_collection_buffer.get_p_mapped_data());

    auto model_transform_ptr =
        static_cast<char*>(get_current_frame().model_transform_set_buffer.get_p_mapped_data());
    uint32_t model_transform_idx{ 0 };

    for (size_t i = 0; i < num_instances; i++)
    {   // would-be outer loop: assign model transform data.
        glm_mat4_copy(const_cast<vec4*>(rend_obj_list[i].transform),
                      reinterpret_cast<vec4*>(model_transform_ptr));
        model_transform_ptr += sizeof(mat4);  // Increment to next model transform.
        model_transform_idx++;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // would-be inner loop where per-instance data is set.

#if 0
        per_inst_data_ptr->material_param_set_idx = 123123;  // Use .material_palette_idx to access the material set, then use the idx of the mesh to access the material param set's idx.
        per_inst_data_ptr->model_transform_set_idx = model_transform_idx - 1;  // Assign a model transform for the model, and then assign that index for the model_transform_set_idx here!
#endif // 0

        // @NOTE: it would also be good to note that the draw lists might be good to build right
        //        here. There are a bunch of assignments to mesh indices and stuff going aorund
        //        here, and to calc it again would likely be a pain, so storing the information here
        //        in memory, separated by shader would be good. Or even just storing that
        //        information into each shader and the draw lists/batches are built in a different
        //        point in time!  -Thea 2026/03/11

        // @NOTE: there needs to be an inner loop since each instance represents a mesh within a
        // model, not the whole model (i.e. render object).  -Thea 2026/03/11


        // @TEMPORARY: in the future do a different setup but for now use this:
        per_inst_data_ptr->material_param_set_idx = 0;
        per_inst_data_ptr->model_transform_set_idx = model_transform_idx - 1;
        ///////////////////////////////////////////////////////////////////////

        per_inst_data_ptr++;  // Increment to next instance.

        ////////////////////////////////////////////////////////////////////////////////////////////
    }
}

void Graphics::Impl::start_next_frame()
{   // Wait until GPU has finished rendering last frame (of current frame index).
    auto& current_frame{ get_current_frame() };

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
                                current_frame.acquire_nxt_img_semaphore,
                                nullptr,
                                &current_swapchain_image_idx);
    if (err)
        throw std::runtime_error("Acquire next swapchain image failed.");

    // Reset command buffers.
    current_frame.graphics_queue_command_buffer.reset();
}

void* Graphics::Impl::get_render_view(size_t rend_view_idx)
{
    return &render_views[rend_view_idx];
}

void Graphics::Impl::blit_image(Vk_Image::Image& from_image,
                                VkExtent3D from_extent,
                                Vk_Image::Image& to_image,
                                VkExtent3D to_extent)
{
    auto cmd{ get_current_frame().graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(cmd,
                                   { { &from_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
                                     { &to_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL } });

    // Blit image.
    VkImageBlit2 blit_region{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets{
            VkOffset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
            VkOffset3D{
                .x = static_cast<int32_t>(from_extent.width),
                .y = static_cast<int32_t>(from_extent.height),
                .z = static_cast<int32_t>(from_extent.depth),
            }
        },
        .dstSubresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets{
            VkOffset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
            VkOffset3D{
                .x = static_cast<int32_t>(to_extent.width),
                .y = static_cast<int32_t>(to_extent.height),
                .z = static_cast<int32_t>(to_extent.depth),
            }
        },
    };

    VkBlitImageInfo2 blit_info{
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = from_image.get(),
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = to_image.get(),
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blit_region,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(cmd, &blit_info);
}

void Graphics::Impl::render_imgui()
{
    auto cmd{ get_current_frame().graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(
        cmd,
        { { &get_current_swapchain_image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } });

    // Render imgui contents to image.
    VkRenderingAttachmentInfo color_attachment =
        Vk_Structs::txp_vk_attachment_info(get_current_swapchain_image_view(),
                                           nullptr,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info =
        Vk_Structs::txp_vk_render_info(gfx.swapchain_extent, &color_attachment, nullptr);

    vkCmdBeginRendering(cmd, &render_info);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);

    // Update and render additional platform windows.
    static auto const& io{ ImGui::GetIO() };
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void Graphics::Impl::present_frame_to_screen()
{
    auto& current_frame{ get_current_frame() };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Change image to present layout.
    Vk_Image::Image::transition_to(
        cmd,
        { { &get_current_swapchain_image(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR } });

    // End recording command buffers.
    current_frame.graphics_queue_command_buffer.finish();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Submit command buffer to queue.
    VkResult err;

    VkSemaphore swapchain_submit_semaphore{ get_current_swapchain_submit_semaphore() };

    VkCommandBufferSubmitInfo cmd_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0,
    };

    VkSemaphoreSubmitInfo wait_info = Vk_Structs::txp_vk_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        current_frame.acquire_nxt_img_semaphore);
    VkSemaphoreSubmitInfo signal_info =
        Vk_Structs::txp_vk_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                 swapchain_submit_semaphore);

    VkSubmitInfo2 submit = Vk_Structs::txp_vk_submit_info(&cmd_info, &signal_info, &wait_info);

    err = vkQueueSubmit2(gfx.graphics_queue, 1, &submit, current_frame.render_fence);
    if (err)
    {
        throw std::runtime_error("Queue submit failed.");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Present image to screen.
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapchain_submit_semaphore,

        .swapchainCount = 1,
        .pSwapchains = &gfx.swapchain,
        .pImageIndices = &current_swapchain_image_idx,
    };

    err = vkQueuePresentKHR(gfx.graphics_queue, &present_info);
    if (err)
    {
        throw std::runtime_error("Queue present KHR failed.");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    current_frame_idx++;  // Increment frame counter for new frame.
}

void TXP::Graphics::Impl::wait_until_gpu_idle()
{
    VkResult err = vkDeviceWaitIdle(gfx.device);
    if (err)
    {
        throw std::runtime_error("Device wait idle failed.");
    }
}

}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
