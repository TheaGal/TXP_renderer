#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

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

#include "gfx_vulkan/vk_image.h"
#include "gfx_vulkan/vk_structs.h"
#include "shader_creation/shader_creation.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>


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

    /// Holds per-frame data.
    struct Frame_data
    {
        VkCommandPool command_pool;
        Command_buffer graphics_queue_command_buffer;
        VkSemaphore acquire_nxt_img_semaphore;
        VkFence render_fence;

        // @TODO: figure out the vv below vv
        // vk_buffer::Allocated_buffer camera_buffer;
        // vk_buffer::GPU_geo_per_frame_buffer geo_per_frame_buffer;
    };
    std::array<Frame_data, k_frame_overlap> frames;

    /// HDR draw image (main geometry pipeline).
    Vk_Image::Allocated_image hdr_draw_image_color;
    Vk_Image::Allocated_image hdr_draw_image_depth;


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
    void init_vulkan_render_graph_resources();
    void init_vulkan_create_descriptors();
    void init_vulkan_create_pipelines();
    void init_vulkan_for_imgui();


    /// Adds textures.
    std::unordered_map<std::string, ktxVulkanTexture> texture_entries;  // @TODO: delete all ktx vk textures. (use `ktxVulkanTexture_Destruct()`)

    ktxVulkanDeviceInfo ktx_vk_device_info;

    void construct_ktx_vk_device_info();
    void destruct_ktx_vk_device_info();

    ktxVulkanTexture load_and_upload_texture(std::string const& fname);
    void add_texture_entry(std::string const& texture_name, ktxVulkanTexture&& allocated_image);


    /// Descriptor binding types.
    using Descriptor_binding_set_t = std::vector<std::pair<uint32_t, VkDescriptorType>>;

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

    private:
        VkDevice m_device;
        VmaAllocator m_allocator;
        VkDescriptorPool m_pool;
    };

    Descriptor_allocator global_descriptor_allocator;

    VkDescriptorSet draw_image_descriptors;               // 3rd (just has to be after 1st)
    VkDescriptorSetLayout draw_image_descriptor_layout;   // 1st (multiple ones)
    VkPipeline draw_image_compute_pipeline;               // 4th (just has to be after 2nd)
    VkPipelineLayout draw_image_compute_pipeline_layout;  // 2nd

    /// Adds shader pipelines.
    struct Shader_pipeline
    {
        std::vector<VkDescriptorSetLayout> descriptor_layouts;
        VkPipelineLayout pipeline_layout;
        std::vector<VkDescriptorSet> descriptor_sets;
        VkPipeline pipeline;
    };
    std::unordered_map<std::string, Shader_pipeline> shader_pipelines;


    /// Polls window for input events.
    void poll_input_events();

    /// Callback for imgui draw.
    std::function<void()> imgui_build_contents_callback;

    void build_imgui_frame();


    /// Index of current frame.
    size_t current_frame_idx{ 0 };
    uint32_t current_swapchain_image_idx;

    void start_new_frame();

    void clear_image(Vk_Image::Image& color_image, Vk_Image::Image& depth_image);
    void draw_compute_thea_custom_hehehe();
    void blit_image(Vk_Image::Image& from_image,
                    VkExtent3D from_extent,
                    Vk_Image::Image& to_image,
                    VkExtent3D to_extent);
    void render_imgui();
    void present_frame_to_screen();

    void wait_until_gpu_idle();
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

void Graphics::Impl::init_vulkan_render_graph_resources()
{
    Vk_Image::Allocated_image::set_vk_props(gfx.physical_device, gfx.device, gfx.allocator);

    {   // HDR draw image.
        VkExtent2D extent{
            .width = 1280,  // @HARDCODE: @THEA: fix this.
            .height = 720,
        };

        hdr_draw_image_color = Vk_Image::Allocated_image::create_image_2d(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            extent,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        hdr_draw_image_depth = Vk_Image::Allocated_image::create_image_depth_buffer(extent);
    }
}

void Graphics::Impl::init_vulkan_create_descriptors()
{
    // @TODO: @THEA: abstract this into the reflection-based version.

    std::vector<Descriptor_allocator::Pool_size_ratio> sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };

    global_descriptor_allocator.init_pool(gfx.device, gfx.allocator, 10, std::move(sizes));

    // Descriptor layouts.
    draw_image_descriptor_layout = build_descriptor_layout(
        {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        },
        VK_SHADER_STAGE_COMPUTE_BIT,
        0);

    // Descriptors.
    draw_image_descriptors = global_descriptor_allocator.allocate(draw_image_descriptor_layout);

    VkDescriptorImageInfo img_info{
        .imageView = hdr_draw_image_color.get_image_view(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet img_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = draw_image_descriptors,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &img_info,
    };

    vkUpdateDescriptorSets(gfx.device, 1, &img_write, 0, nullptr);
}

void Graphics::Impl::init_vulkan_create_pipelines()
{
    // @TODO: @THEA: abstract this into the reflection-based version.

    VkResult err;

    // Create pipeline layout.
    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = 1,
        .pSetLayouts = &draw_image_descriptor_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };

    err = vkCreatePipelineLayout(gfx.device,
                                 &pipeline_layout_info,
                                 nullptr,
                                 &draw_image_compute_pipeline_layout);
    if (err)
        throw std::runtime_error("Failed to create pipeline layout.");

    // Create pipeline.
    VkShaderModule shader_module{ load_shader_module("assets/shaders/gradient.shader") };

    VkPipelineShaderStageCreateInfo stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = "compute_main",
        // .pSpecializationInfo = nullptr,  // @RESEARCH: research this if you want!
    };

    VkComputePipelineCreateInfo compute_pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = stage_info,
        .layout = draw_image_compute_pipeline_layout,
    };

    err = vkCreateComputePipelines(gfx.device,
                                   VK_NULL_HANDLE,
                                   1,
                                   &compute_pipeline_info,
                                   nullptr,
                                   &draw_image_compute_pipeline);
    if (err)
        throw std::runtime_error("Failed to create compute pipeline.");

    // Cleanup.
    vkDestroyShaderModule(gfx.device, shader_module, nullptr);
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


std::vector<Graphics::Impl::Descriptor_binding_set_t> Graphics::Impl::
    get_descriptor_binding_sets_from_shader_properties(Shader_Creation::Extracted_info const& info,
                                                       Shader_Creation::Shader_pipeline_type type)
{
    auto stage_names = Shader_Creation::get_shader_pipeline_stage_names(type);

    std::vector<Descriptor_binding_set_t> binding_sets;
    binding_sets.reserve(stage_names.size());

    for (auto const& stage_name : stage_names)
    {
        for (auto const& entry_pt : info.entry_points)
            if (entry_pt.entry_point_stage == stage_name)
            {   // Process this entry point.
                binding_sets.emplace_back();

                for (auto const& [_, desc_binding] :
                     entry_pt.desc_layout_info.shader_param_to_layout_binding)
                    binding_sets.back().emplace_back(desc_binding.binding_idx,
                                                     VkDescriptorType(desc_binding.desc_type));

                break;  // Goto next stage name.
            }
    }
    if (stage_names.size() != binding_sets.size())
        throw std::runtime_error("Count of binding set is mismatched from expected.");

    return binding_sets;
}

VkShaderStageFlags Graphics::Impl::get_stage_flags_from_shader_type(
    Shader_Creation::Shader_pipeline_type type)
{
    switch (type)
    {
    case Shader_Creation::SHAD_PIPE_TYPE_COMPUTE:
        return VK_SHADER_STAGE_COMPUTE_BIT;

    case Shader_Creation::SHAD_PIPE_TYPE_VERTEX_FRAGMENT:
        return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    default:
        throw std::runtime_error("Unsupported shader stage flag.");
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

    for (auto [bind_idx, descriptor_type] : bindings)
    {
        layout_bindings.emplace_back(VkDescriptorSetLayoutBinding{
            .binding = bind_idx,
            .descriptorType = descriptor_type,
            .descriptorCount = 1,
            .stageFlags = shader_stages,
        });
    }

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
        .pBindings = layout_bindings.data(),
    };

    VkDescriptorSetLayout layout;
    VkResult err = vkCreateDescriptorSetLayout(gfx.device,
                                               &info,
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

void Graphics::Impl::build_imgui_frame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Frame contents.
    ImGui::ShowDemoWindow();  // @TODO: erase this
    // if (!imgui_build_contents_callback)  @TODO
    //     throw std::runtime_error("ImGui build contents callback not defined!");
    // imgui_build_contents_callback();

    // Convert to render instructions.
    ImGui::Render();
}

void Graphics::Impl::start_new_frame()
{   // Wait until GPU has finished rendering last frame (of current frame index).
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
                                current_frame.acquire_nxt_img_semaphore,
                                nullptr,
                                &current_swapchain_image_idx);
    if (err)
        throw std::runtime_error("Acquire next swapchain image failed.");

    // Reset command buffers.
    current_frame.graphics_queue_command_buffer.reset();
}

void Graphics::Impl::clear_image(Vk_Image::Image& color_image, Vk_Image::Image& depth_image)
{
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(
        cmd,
        { { &color_image, VK_IMAGE_LAYOUT_GENERAL },
          { &depth_image, VK_IMAGE_LAYOUT_GENERAL } });

    // Clear color image.
    float_t flash = std::abs(std::sin(current_frame_idx / 120.f));
    VkClearColorValue clear_value{ .float32 = { 0.0f, 0.0f, flash * 0.5f, 1.0f } };
    VkImageSubresourceRange clear_range =
        Vk_Structs::txp_vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(cmd,
                         color_image.get(),
                         VK_IMAGE_LAYOUT_GENERAL,
                         &clear_value,
                         1,
                         &clear_range);

    // Clear depth image.
    VkClearDepthStencilValue clear_depth_value{
        .depth = 0.0f,
    };
    VkImageSubresourceRange clear_depth_range =
        Vk_Structs::txp_vk_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);

    vkCmdClearDepthStencilImage(cmd,
                                depth_image.get(),
                                VK_IMAGE_LAYOUT_GENERAL,
                                &clear_depth_value,
                                1,
                                &clear_depth_range);
}

void Graphics::Impl::draw_compute_thea_custom_hehehe()
{
    // @TODO: @THEA: abstract this into the reflection-based version.

    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };


    Vk_Image::Image::transition_to(
        cmd,
        { { &hdr_draw_image_color.get_image(), VK_IMAGE_LAYOUT_GENERAL } });  // @THEA: @TEMP: this is @HARDCODE

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, draw_image_compute_pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            draw_image_compute_pipeline_layout,
                            0,
                            1, &draw_image_descriptors,
                            0, nullptr);

    // "threadGroupSize": [16, 16, 1],
    vkCmdDispatch(cmd, std::ceil(1280.0 / 16.0), std::ceil(720.0 / 16.0), 1);  // @HARDCODE: @THEA: fix this.
}

void Graphics::Impl::blit_image(Vk_Image::Image& from_image,
                                VkExtent3D from_extent,
                                Vk_Image::Image& to_image,
                                VkExtent3D to_extent)
{
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

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
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto& image{ gfx.swapchain_images[current_swapchain_image_idx] };  // @TODO: make this `image` a param at some point.
    auto image_view{ gfx.swapchain_image_views[current_swapchain_image_idx] };  // @TODO: make this `image_view` a param at some point.

    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(cmd, { { &image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } });

    // Render imgui contents to image.
    VkRenderingAttachmentInfo color_attachment =
        Vk_Structs::txp_vk_attachment_info(image_view,
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
    auto& current_frame{ frames[current_frame_idx % k_frame_overlap] };
    auto& image{ gfx.swapchain_images[current_swapchain_image_idx] };  // @TODO: make this `image` a param at some point.

    auto cmd{ current_frame.graphics_queue_command_buffer.get() };

    // Change image to present layout.
    Vk_Image::Image::transition_to(cmd, { { &image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR } });

    // End recording command buffers.
    current_frame.graphics_queue_command_buffer.finish();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Submit command buffer to queue.
    VkResult err;

    VkSemaphore swapchain_submit_semaphore{
        gfx.swapchain_submit_semaphores[current_swapchain_image_idx]
    };

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
    m_pimpl->select_vulkan_window_surface_format();
    m_pimpl->init_vulkan_build_swapchain();
    m_pimpl->init_vulkan_retrieve_queues();
    m_pimpl->init_vulkan_create_cmd_structures();
    m_pimpl->init_vulkan_create_sync_structures();
    m_pimpl->init_vulkan_render_graph_resources();
    m_pimpl->init_vulkan_create_descriptors();
    m_pimpl->init_vulkan_create_pipelines();
    m_pimpl->init_vulkan_for_imgui();
}

TXP::Graphics::~Graphics()
{
    // m_pimpl->destroy_texture_entries();  @TODO

    // @TODO
    assert(false);
}

void TXP::Graphics::load_assets(std::string const& texture_asset_dir,
                                std::string const& shader_asset_dir,
                                std::string const& model_asset_dir,
                                std::vector<Texture_asset_create_info>&& texture_assets,
                                std::vector<Material_asset_create_info>&& material_assets,
                                std::vector<Material_set_asset_create_info>&& material_set_assets,
                                std::vector<Model_asset_create_info>&& model_assets)
{   // Load textures.
    m_pimpl->construct_ktx_vk_device_info();
    for (auto const& tex_asset : texture_assets)
    {
        m_pimpl->add_texture_entry(
            tex_asset.texture_name,
            m_pimpl->load_and_upload_texture(texture_asset_dir + tex_asset.ktx2_fname));
    }
    m_pimpl->destruct_ktx_vk_device_info();
    std::cout << "Loaded all " << std::to_string(texture_assets.size()) << " textures.\n";

    // Collect required shaders.
    std::set<std::pair<std::string, Shader_Creation::Shader_pipeline_type>> shader_names_and_types;
    for (auto const& mat_asset : material_assets)
        shader_names_and_types.emplace(mat_asset.shader_name_and_type);
    std::cout << "Found usage of " << std::to_string(shader_names_and_types.size())
              << " shaders." << std::endl;

    // Load shaders.
    Shader_Creation::set_shader_directory(shader_asset_dir);
    Shader_Creation::clear_slang_reflection_collection();
    for (auto const& [shad_name, _] : shader_names_and_types)
        Shader_Creation::load_slang_reflection_into_collection(shad_name);
    for (auto const& [shad_name, shad_type] : shader_names_and_types)
    {


        // Extract shader properties.
        auto shader_properties{ Shader_Creation::extract_stuff(shad_name, shad_type) };
        auto binding_sets{ m_pimpl->get_descriptor_binding_sets_from_shader_properties(
            shader_properties,
            shad_type) };

        // Create descriptor layouts.
        for (auto& binding_set : binding_sets)
            m_pimpl->build_descriptor_layout(std::move(binding_set),
                                             m_pimpl->get_stage_flags_from_shader_type(shad_type),
                                             0);
    }

    std::cout << "Loaded all " << std::to_string(shader_names_and_types.size()) << " shaders.\n";

    // Load materials.
    for (auto const& mat_asset : material_assets)
    {

    }
    std::cout << "Loaded all " << std::to_string(material_assets.size()) << " materials.\n";

    // Load material sets.
    for (auto const& mat_set_asset : material_set_assets)
    {

    }
    std::cout << "Loaded all " << std::to_string(material_set_assets.size()) << " material sets.\n";

    // Load models.
    for (auto const& mod_asset : model_assets)
    {

    }
    std::cout << "Loaded all " << std::to_string(model_assets.size()) << " models.\n";
}

void TXP::Graphics::poll_input_events()
{
    m_pimpl->poll_input_events();
}

void TXP::Graphics::build_imgui_frame()
{
    m_pimpl->build_imgui_frame();
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
    m_pimpl->clear_image(m_pimpl->hdr_draw_image_color.get_image(),
                         m_pimpl->hdr_draw_image_depth.get_image());
    m_pimpl->draw_compute_thea_custom_hehehe();
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
    auto const& swapchain_extent{ m_pimpl->gfx.swapchain_extent };
    m_pimpl->blit_image(m_pimpl->hdr_draw_image_color.get_image(),
                        m_pimpl->hdr_draw_image_color.get_extent(),
                        m_pimpl->gfx.swapchain_images[m_pimpl->current_swapchain_image_idx],
                        VkExtent3D{ .width = swapchain_extent.width,
                                    .height = swapchain_extent.height,
                                    .depth = 1 });
}

void TXP::Graphics::render_imgui()
{
    m_pimpl->render_imgui();
}

void TXP::Graphics::present_frame_to_screen()
{
    m_pimpl->present_frame_to_screen();
}

void TXP::Graphics::wait_until_gpu_idle()
{
    m_pimpl->wait_until_gpu_idle();
}

}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
