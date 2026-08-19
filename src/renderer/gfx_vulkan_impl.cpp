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
#include "ImGuizmo.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#define KHRONOS_STATIC 1
#include "ktx.h"
#include "ktxvulkan.h"
// clang-format on

#include "btdatecheck.h"
#include "btglm.h"
#include "btlogger.h"
#include "btservice_finder.h"
#include "editor_conent/editor_content.h"
#include "gfx_vulkan/vk_image.h"
#include "gfx_vulkan/vk_structs.h"
#include "material_organizer/material_organizer.h"
#include "render_object/deformed_render_model.h"
#include "render_object/render_model.h"
#include "render_object/vertex.h"
#include "renderer/gfx.h"
#include "renderer/types.h"
#include "txp_renderer/input_handler/input_handler.h"
#include "txp_renderer/renderer.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
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

static void window_resize_callback(GLFWwindow* window, int32_t width, int32_t height)
{
    s_input_handler->window_resize_event(width, height);
}

static void window_content_scale_callback(GLFWwindow* window, float_t xscale, float_t yscale)
{
    BT_WARNF("%s(%p, %f, %f)", __func__, window, xscale, yscale);
}

static void window_maximize_callback(GLFWwindow* window, int32_t maximized)
{
    BT_WARNF("%s(%p, %i)", __func__, window, maximized);
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

void Graphics::Impl::init_window_props(bool pre_creation)
{
    assert(!settings.is_fullscreen);  // @TODO: implement fullscreen stuff in the future!!

    int32_t window_dims[2]{
        settings.windowed_width,
        settings.windowed_height,
    };
    assert(window_dims[0] > 0 && window_dims[1] > 0);

    GLFWmonitor* target_monitor{ nullptr };
    {
        int32_t monitor_count;
        auto** monitors{ glfwGetMonitors(&monitor_count) };
        if (settings.monitor_idx >= 0 && settings.monitor_idx < monitor_count)
        {
            target_monitor = monitors[settings.monitor_idx];
        }
        else
        {
            // Reset monitor idx setting.
            settings.monitor_idx = 0;
            target_monitor = glfwGetPrimaryMonitor();
        }
    }

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

    if (pre_creation)
    {
        glfwWindowHint(GLFW_POSITION_X, centered_window_pos[0]);
        glfwWindowHint(GLFW_POSITION_Y, centered_window_pos[1]);

        // Get monitor scaling.
        monitor_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(target_monitor);

        glfwWindowHint(GLFW_RESIZABLE, settings.is_resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, settings.has_border ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, settings.is_maximized ? GLFW_TRUE : GLFW_FALSE);

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }
    else
    {
        assert(window != nullptr);

        if (settings.is_fullscreen)
        {
            assert(false);  // @TODO: implement.
        }
        else
        {
            glfwSetWindowMonitor(window,
                                 nullptr,
                                 centered_window_pos[0],
                                 centered_window_pos[1],
                                 window_dims[0],
                                 window_dims[1],
                                 GLFW_DONT_CARE);
        }
    }
}

void Graphics::Impl::init_window()
{
    int32_t window_dims[2]{
        settings.windowed_width,
        settings.windowed_height,
    };
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
    glfwSetWindowSizeCallback(window, window_resize_callback);
    glfwSetWindowContentScaleCallback(window, window_content_scale_callback);
    glfwSetWindowMaximizeCallback(window, window_maximize_callback);
    glfwSetWindowCloseCallback(window, window_close_callback);

    // Lock cursor func.
    lock_cursor_fn = [&](bool lock) {
        // Enable/disable ImGui reading mouse and keyboard interactions.
        if (lock)  ImGui::GetIO().ConfigFlags |= (ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);
        else       ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);

        glfwSetInputMode(window, GLFW_CURSOR, lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    };

    // Finish.
    glfwShowWindow(window);
}

void Graphics::Impl::destroy_glfw()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}


void Graphics::Impl::request_load_settings()
{
    load_settings_flag = true;
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

    // Check for if doing a swapchain rebuild.
    bool building_brand_new_swapchain{ gfx.swapchain == VK_NULL_HANDLE };
    VkSwapchainKHR old_swapchain{ gfx.swapchain };  // Will be NULL if new swapchain.

    // Build swapchain.
    vkb::SwapchainBuilder swapchain_builder{ gfx.physical_device, gfx.device, gfx.surface };
    vkb::Swapchain swapchain{
        swapchain_builder
            .set_old_swapchain(old_swapchain)  // @NOTE: first init will be NULL_HANDLE.
            .set_desired_extent(fb_width, fb_height)
            .set_desired_format(gfx.surface_format)
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // G-Sync.
            .add_fallback_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)  // Freesync / V-Sync off.
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)    // V-Sync on.
            // @TODO: TRANSFER_DST image usage added below. Try removing once renderer is finished
            // (assuming you're not gonna have some kind of image transfer as the last step into the
            // swapchain image).
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value()
    };

    if (!building_brand_new_swapchain)
    {   // Delete old stuff (once old swapchain got used for the rebuild).
        vkDestroySwapchainKHR(gfx.device, old_swapchain, nullptr);
        for (auto img_view : gfx.swapchain_image_views)
        {
            vkDestroyImageView(gfx.device, img_view, nullptr);
        }
        gfx.swapchain_images.clear();
        gfx.swapchain_image_views.clear();
    }

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

    if (building_brand_new_swapchain)
        gfx.swapchain_submit_semaphores.resize(gfx.swapchain_images.size());
    else if (gfx.swapchain_submit_semaphores.size() != gfx.swapchain_images.size())
        throw std::runtime_error(
            "Swapchain image count changed. No handling implemented for this case!!");

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

    for (auto& sss : gfx.swapchain_submit_semaphores)
    {
        sss = create_semaphore();
    }

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateFence(gfx.device, &fence_create_info, nullptr, &frames[i].render_fence);
        if (err)
        {
            throw std::runtime_error("Vulkan render fence creation failed.");
        }

        frames[i].acquire_nxt_img_semaphore = create_semaphore();
    }
}

void Graphics::Impl::init_vulkan_for_imgui()
{
    {   // Create descriptor pool.
        VkDescriptorPoolSize pool_sizes[]{
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1024 },  // @NOCHECKIN: This may be unnecessary since desc pool stuff hasn't gone over yet???
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

    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
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
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
    };

    global_descriptor_allocator.init_pool(gfx.device, gfx.allocator, 10, std::move(sizes));
}

void Graphics::Impl::rebuild_vulkan_swapchain()
{
    wait_until_gpu_idle();
    init_vulkan_build_swapchain();
}

void Graphics::Impl::destroy_vulkan()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(gfx.device, gfx.imgui_desc_pool, nullptr);

    for (auto sss : gfx.swapchain_submit_semaphores)
    {
        vkDestroySemaphore(gfx.device, sss, nullptr);
    }
    for (auto& frame : frames)
    {
        vkDestroyFence(gfx.device, frame.render_fence, nullptr);
        vkDestroySemaphore(gfx.device, frame.acquire_nxt_img_semaphore, nullptr);
    }

    for (auto& frame : frames)
    {
        for (auto& env_data_buffer : frame.environment_data_buffers)
        {
            env_data_buffer.destroy();
        }
        frame.per_instance_data_collection_buffer.destroy();
        frame.model_transform_set_buffer.destroy();
    }

    combined_static_model.vertex_index_buffer.destroy();
    combined_deformed_model.vertex_index_buffer.destroy();

    for (auto& render_view : render_views)
    {
        render_view.color_image.teardown();
        render_view.depth_image.teardown();
    }
    vkDestroySampler(gfx.device, render_view_imgui_image_sampler, nullptr);

    for (auto desc_layout : built_descriptor_layouts)
    {
        vkDestroyDescriptorSetLayout(gfx.device, desc_layout, nullptr);
    }
    global_descriptor_allocator.teardown_pool();

    for (auto& frame : frames)
    {
        vkDestroyCommandPool(gfx.device, frame.command_pool, nullptr);
    }

    vmaDestroyAllocator(gfx.allocator);

    // Destroy swapchain.
    vkDestroySwapchainKHR(gfx.device, gfx.swapchain, nullptr);
    for (auto img_view : gfx.swapchain_image_views)
    {
        vkDestroyImageView(gfx.device, img_view, nullptr);
    }

    vkDestroySurfaceKHR(gfx.instance, gfx.surface, nullptr);
    vkb::destroy_debug_utils_messenger(gfx.instance, gfx.debug_utils_messenger);

    vkDestroyDevice(gfx.device, nullptr);
    vkDestroyInstance(gfx.instance, nullptr);
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

void Graphics::Impl::destroy_texture_entries()
{
    for (auto& [_, texture_entry] : texture_entries)
    {
        vkDestroySampler(gfx.device, texture_entry.sampler, nullptr);
        vkDestroyImageView(gfx.device, texture_entry.image_view, nullptr);
        ktxVulkanTexture_Destruct(&texture_entry.texture, gfx.device, nullptr);
    }
}


void Graphics::Impl::upload_model_entries_to_gpu(
    Render_model_data_collection& data_collection)
{   // Get static model information.
    // @TODO: change this to getting a list of model-data-set indexes, instead of getting names then translating list of names to indexes.
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

    for (auto* model : static_models)
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

    for (auto* model : static_models)
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

    if (vertex_buf_size == 0)
    {
        BT_WARN("No models to upload in combined static model.");
        return;
    }

    combined_static_model.vertex_index_buffer.create(
        gfx.device,
        gfx.allocator,
        vertex_buf_size + index_buf_size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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

void Graphics::Impl::upload_model_skins_to_gpu(Render_model_data_collection& data_collection)
{   // Get static model information.
    // @TODO: change this to getting a list of model-skin indexes, instead of getting names then translating list of names to indexes.
    std::vector<std::string> model_skin_names =
        data_collection.get_deformed_model_skin_name_list();

    std::vector<Deformed_model_skin*> model_skins;
    model_skins.reserve(model_skin_names.size());

    for (auto const& name : model_skin_names)
        model_skins.emplace_back(
            const_cast<Deformed_model_skin*>(&data_collection.get_deformed_model_skin(
                data_collection.get_deformed_model_skin_idx(name))));

    // Process each model skin.
    for (auto* model_skin : model_skins)
    {
        // Create buffer for model skins.
        model_skin->vert_skin_data_buffer.create(
            gfx.device,
            gfx.allocator,
            model_skin->vert_skin_datas.size() * sizeof(Vertex_skin_data),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        // Upload data.
        std::memcpy(model_skin->vert_skin_data_buffer.get_p_mapped_data(),
                    model_skin->vert_skin_datas.data(),
                    model_skin->vert_skin_datas.size() * sizeof(Vertex_skin_data));
    }
}

void Graphics::Impl::build_deformed_combined_model(
        Render_model_data_collection& data_collection)  // @COPYPASTA of `upload_model_entries_to_gpu()`
{   // Get deformed model information.
    // @TODO: change this to getting a list of model-data-set indexes, instead of getting names then translating list of names to indexes.
    std::vector<Deformed_model_data_set*> deformed_models =
        data_collection.get_all_deformed_models();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Transform to singular deformed model.

    // Reserve space for vertices and indices.
    size_t vertex_count{ 0 };
    size_t index_count{ 0 };

    for (auto* model : deformed_models)
    {
        // @NOTE: using `vertex_count` for offset since that is the base of the vertex index.
        model->deformed_model.vertex_index_offset = vertex_count;

        auto const& base_static_model{ data_collection.get_static_model_data_set(
            model->base_static_model_idx) };
        vertex_count += base_static_model.vertices.size();

        for (auto& mesh : model->deformed_model.meshes)
        {   // Mark where to start in index buffer for this mesh.
            model->deformed_model.first_index_offsets.emplace_back(index_count);

            index_count += mesh.indices.size();
        }
    }

    // Copy vertices and indices.
    std::vector<Vertex> combined_vertices(vertex_count);
    std::vector<uint32_t> combined_indices(index_count);

    vertex_count = 0;
    index_count = 0;

    for (auto const* model : deformed_models)
    {
        auto const& base_static_model{ data_collection.get_static_model_data_set(
            model->base_static_model_idx) };

        std::memcpy(combined_vertices.data() + vertex_count,
                    base_static_model.vertices.data(),
                    base_static_model.vertices.size() * sizeof(Vertex));
        vertex_count += base_static_model.vertices.size();

        for (auto const& mesh : model->deformed_model.meshes)
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

    if (vertex_buf_size == 0)
    {
        BT_WARN("No models to upload in combined static model.");
        return;
    }

    if (combined_deformed_model.vertex_index_buffer.is_created())
        combined_deformed_model.vertex_index_buffer.destroy();  // Destroy before recreating.

    combined_deformed_model.vertex_index_buffer.create(
        gfx.device,
        gfx.allocator,
        vertex_buf_size + index_buf_size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    combined_deformed_model.offset_to_index_buffer = vertex_buf_size;

    void* p_mapped_data{ combined_deformed_model.vertex_index_buffer.get_p_mapped_data() };
    auto offset_to_idx_buf{ combined_deformed_model.offset_to_index_buffer };

    std::memcpy(p_mapped_data,
                combined_vertices.data(),
                vertex_buf_size);
    std::memcpy(reinterpret_cast<char*>(p_mapped_data) + offset_to_idx_buf,
                combined_indices.data(),
                index_buf_size);
}

void Graphics::Impl::create_joint_transforms_buffers(Render_model_data_collection& data_collection)
{
    for (auto* def_mod : data_collection.get_all_deformed_models())
    {
        if (!def_mod->joint_transforms_buffer.is_created())
        {   // Create joint transforms buffer.
            def_mod->joint_transforms_buffer.create(
                gfx.device,
                gfx.allocator,
                def_mod->model_skin.joints_sorted_breadth_first.size() * sizeof(mat4s),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);

            // Load identity transform onto GPU.
            for (size_t i = 0; i < def_mod->model_skin.joints_sorted_breadth_first.size(); i++)
            {
                auto* joint_trans_buf_ptr{ static_cast<mat4s*>(
                    def_mod->joint_transforms_buffer.get_p_mapped_data()) };
                glm_mat4_identity(joint_trans_buf_ptr->raw);
                joint_trans_buf_ptr++;
            }
        }
    }
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

    built_descriptor_layouts.emplace_back(layout);

    return layout;
}


void Graphics::Impl::create_all_textures_descriptor()
{   // Create all-texture descriptor image infos.
    all_texture_infos.reserve(texture_entries.size());

    if (texture_entries.empty())
    {
        BT_ERROR(
            "Cannot create descriptor set with empty texture entries. Add entries or else I will "
            "be very disappointed. Not mad, just disappointed.");
        throw std::runtime_error("No texture entries.");
    }
    if (texture_entries.size() > 16)
    {
        BT_ERRORF(
            "Texture entries exceeded macOS limit of maxPerStageDescritorSamplers: %zu. At "
            "this point, separate out the COMBINED_IMAGE_SAMPLERS thingies into sampled images "
            "and samplers. Change this error message to just error when the number of samplers "
            "goes over whatever the device limit is, or 16, whichever is smaller.",
            texture_entries.size());
        throw std::runtime_error("Too many samplers.");
    }
    if (texture_entries.size() > 256)
    {
        BT_ERRORF(
            "Texture entries exceeded macOS limit of maxPerStageDescritorSampledImages: %zu. "
            "At this point, you need to make a material-sampledimage-sampler batcher that can "
            "handle the sampler and sampledimage device limits. Once you make this, uncap the "
            "sampler limit from the random 16 limit. Also, change this error message to error "
            "if the sampled image size exceeds device limits once you implement the batcher "
            "system. Good luck, future Thea!!",
            texture_entries.size());
        throw std::runtime_error("Too many sampled images.");
    }

    std::vector<VkDescriptorImageInfo> desc_img_infos;
    desc_img_infos.reserve(texture_entries.size());

    for (auto& [name, entry] : texture_entries)
    {
        VkResult err;

        // Create texture sampler.
        VkSamplerCreateInfo sampler_info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = (entry.texture.levelCount == 1 ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                         : VK_SAMPLER_MIPMAP_MODE_LINEAR),
            .anisotropyEnable = VK_TRUE,  // @TODO: put this into a setting!
            .maxAnisotropy = 8.0f,  // 8 is a widely supported value for max anisotropy.  @TODO: put this into a setting!
            .maxLod = static_cast<float>(entry.texture.levelCount),
        };

        err = vkCreateSampler(gfx.device, &sampler_info, nullptr, &entry.sampler);
        if (err)
            throw std::runtime_error("Failed to create VK sampler.");

        // Create texture image view.
        VkImageViewCreateInfo image_view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = entry.texture.image,
            .viewType = entry.texture.viewType,
            .format = entry.texture.imageFormat,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = entry.texture.levelCount,
                .baseArrayLayer = 0,
                .layerCount = entry.texture.layerCount,
            },
        };

        err = vkCreateImageView(gfx.device, &image_view_info, nullptr, &entry.image_view);
        if (err)
            throw std::runtime_error("Failed to create VK image view.");

        // Create descriptor.
        VkDescriptorImageInfo desc_img_info{
            .sampler = entry.sampler,
            .imageView = entry.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        entry.gpu_idx = desc_img_infos.size();
        desc_img_infos.emplace_back(std::move(desc_img_info));
    }

    // Descriptor layouts.
    using Descriptor_type_info = Graphics::Impl::Descriptor_type_info;

    all_textures_descriptor_layout = build_descriptor_layout(
        {
            { 0,
              Descriptor_type_info{ .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    .use_variable_descriptor_count_binding_flag = true,
                                    .variable_descriptor_count =
                                        static_cast<uint32_t>(texture_entries.size()) } },
        },
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0);

    // Descriptors.
    all_textures_descriptor_set =
        global_descriptor_allocator.allocate(all_textures_descriptor_layout,
                                             static_cast<uint32_t>(texture_entries.size()));

    VkWriteDescriptorSet imgs_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = all_textures_descriptor_set,
                                     .dstBinding = 0,
                                     .descriptorCount =
                                         static_cast<uint32_t>(texture_entries.size()),
                                     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .pImageInfo = desc_img_infos.data() };
    vkUpdateDescriptorSets(gfx.device, 1, &imgs_write, 0, nullptr);
}


void Graphics::Impl::poll_input_events()
{
    if (load_settings_flag)
    {
        load_settings_flag = false;
        init_window_props(false);
    }

    glfwPollEvents();
}

void Graphics::Impl::build_imgui_contents(Camera_internal& camera,
                                          std::vector<Render_view_size>& out_rend_view_sizes)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());  // @TEMPORARY: move imguizmo to draw for each window eventually.
    BT::date_deadline(2026, 8, 30);

    editor_content::Render_view_image_content render_view_image_content;
    render_view_image_content.content_image_descriptors.reserve(render_views.size());
    for (auto const& rv : render_views)
    {
        render_view_image_content.content_image_descriptors.emplace_back(
            rv.imgui_color_image_descriptor);
    }

    editor_content::build_content(*s_input_handler,
                                  render_view_image_content,
                                  lock_cursor_fn,
                                  camera,
                                  info_hook_struct,
                                  out_rend_view_sizes);

    // Convert to render instructions.
    ImGui::Render();
}

bool Graphics::Impl::check_render_view_sizes_changed(
    std::vector<Render_view_size> const& rend_view_sizes) const
{
    if (rend_view_sizes.size() != render_views.size())
        return true;
    
    for (size_t i = 0; i < rend_view_sizes.size(); i++)
    {
        if (rend_view_sizes[i].width != render_views[i].color_image.get_extent().width ||
            rend_view_sizes[i].height != render_views[i].color_image.get_extent().height)
            return true;
    }

    return false;
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

    // Disable buffer deletion check (since moving from the container resizing).
    for (auto& b : current_frame.environment_data_buffers)
        b.set_created_check(false);

    // Destroy environment data buffers if shrinking its list's size.
    for (size_t i = rend_view_sizes.size(); i < prev_env_data_buffers_size; i++)
    {
        ensure_gpu_idle_fn();
        current_frame.environment_data_buffers[i].destroy();
        current_frame.environment_data_buffers[i].set_created_check(true);  // Enable bc deleting.
    }

    // Resize.
    render_views.resize(rend_view_sizes.size());
    current_frame.environment_data_buffers.resize(rend_view_sizes.size());

    // Re-enable buffer deletion check.
    for (auto& b : current_frame.environment_data_buffers)
        b.set_created_check(false);

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
    Material_organizer const& material_organizer,
    std::vector<Render_object> const& rend_obj_list,
    std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
    size_t mod_mesh_ref_list_length)
{
    // Write transform data.
    size_t num_transforms{ std::min(rend_obj_list.size(), static_cast<size_t>(65535)) };  // @HARDCODE: comes from shader.
    auto model_transform_ptr =
        static_cast<char*>(get_current_frame().model_transform_set_buffer.get_p_mapped_data());

    for (size_t i = 0; i < num_transforms; i++)
    {
        auto const& rend_obj{ rend_obj_list[i] };
        if (rend_obj.sub_mesh_idx != (uint16_t)-1 && rend_obj.sub_mesh_zero_origin_position)
        {
            // Hey so, it turns out that sub-mesh origin position zeroing doesn't work due to
            // limitations to the gltf importer (wonkiness) and the wavefront obj file format (no
            // origins preserved in file format). So this is essentially @DEPRECATED
            //   -Thea 2026/08/04
            assert(false);

            // @TODO: @CHECK: is this branch more expensive or doing the extra mat4 multiplication?
            //                  -Thea 2026/08/04
            vec3 inv_orig_pos;
            glm_vec3_negate_to(const_cast<float_t*>(rend_obj.sub_mesh_origin_position),
                               inv_orig_pos);

            mat4 zero_out_trans;
            glm_translate_make(zero_out_trans, inv_orig_pos);

            glm_mat4_mul(zero_out_trans,
                         const_cast<vec4*>(rend_obj.transform),
                         reinterpret_cast<vec4*>(model_transform_ptr));
        }
        else
        {
            glm_mat4_copy(const_cast<vec4*>(rend_obj.transform),
                          reinterpret_cast<vec4*>(model_transform_ptr));
        }
        model_transform_ptr += sizeof(mat4);  // Increment to next model transform.
    }

    // Write per-instance data.
    size_t num_instances{ std::min(mod_mesh_ref_list_length, static_cast<size_t>(65535)) };  // @HARDCODE: comes from shader.
    auto per_inst_data_ptr = static_cast<gpu_type::Per_instance_data*>(
        get_current_frame().per_instance_data_collection_buffer.get_p_mapped_data());

    for (size_t i = 0; i < num_instances; i++)
    {
        auto const& modmesh_ref{ model_mesh_ref_list[i] };
        auto rend_obj_idx{ modmesh_ref.render_obj_idx };
        auto mat_pal_idx{ rend_obj_list[rend_obj_idx].material_palette_idx };

        auto material_param_set_idx{ material_organizer.get_material_palette(mat_pal_idx)
                                         .at(modmesh_ref.model_mesh_idx)
                                         .material_param_set_idx };

        per_inst_data_ptr->model_transform_set_idx = rend_obj_idx;
        per_inst_data_ptr->material_param_set_idx = material_param_set_idx;

        per_inst_data_ptr++;  // Increment to next instance.
    }
}

bool Graphics::Impl::start_next_frame()
{   // Wait until GPU has finished rendering last frame (of current frame index).
    auto& current_frame{ get_current_frame() };

    constexpr uint64_t k_10sec_as_ns{ 10'000'000'000 };

    VkResult err;

    // Sync fences before readying command buffers.
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

    // Acquire next image and check for swapchain recreation requirements.
    do
    {
        current_swapchain_image_idx = (uint32_t)-1;  // @DEBUG
        err = vkAcquireNextImageKHR(gfx.device,
                                    gfx.swapchain,
                                    k_10sec_as_ns,
                                    current_frame.acquire_nxt_img_semaphore,
                                    nullptr,
                                    &current_swapchain_image_idx);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            rebuild_vulkan_swapchain();

            destroy_semaphore(current_frame.acquire_nxt_img_semaphore);
            current_frame.acquire_nxt_img_semaphore = create_semaphore();

            if (err == VK_ERROR_OUT_OF_DATE_KHR)
                return false;
        }
        else if (err)
            throw std::runtime_error("Acquire next swapchain image failed.");
    } while (err != VK_SUCCESS);

    // Reset command buffers.
    current_frame.graphics_queue_command_buffer.reset();

    return true;
}

void* Graphics::Impl::get_render_view(size_t rend_view_idx)
{
    return &render_views[rend_view_idx];
}

void Graphics::Impl::begin_rendering_render_view(size_t rend_view_idx)
{   
    VkCommandBuffer cmd{ get_current_frame().graphics_queue_command_buffer.get() };
    auto& color_image{ render_views[rend_view_idx].color_image };
    auto& depth_image{ render_views[rend_view_idx].depth_image };

    // Ready images.
    Vk_Image::Image::transition_to(
        cmd,
        { { &color_image.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },
          { &depth_image.get_image(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL } });

    // Begin rendering.
    VkClearValue color_clear_value{
        .color{ .float32{ 0, 0, 0, 1 } },
    };
    VkRenderingAttachmentInfo color_attachment =
        Vk_Structs::txp_vk_attachment_info(color_image.get_image_view(),
                                           &color_clear_value,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkClearValue depth_clear_value{
        .depthStencil{ .depth = 1.0f, .stencil = 0 },
    };
    VkRenderingAttachmentInfo depth_attachment =
        Vk_Structs::txp_vk_attachment_info(depth_image.get_image_view(),
                                           &depth_clear_value,
                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = Vk_Structs::txp_vk_render_info(
        VkExtent2D{ .width = color_image.get_extent().width,
                    .height = color_image.get_extent().height },
        &color_attachment,
        &depth_attachment);
    vkCmdBeginRendering(cmd, &render_info);
}

void Graphics::Impl::end_rendering_render_view()
{   // End rendering.
    vkCmdEndRendering(get_current_frame().graphics_queue_command_buffer.get());
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
