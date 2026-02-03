#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <GLFW/glfw3.h>
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
        // std::cerr << "ERROR: Window creation failed." << std::endl;
        glfwTerminate();
        assert(false);
        return nullptr;
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

void init_vulkan(GLFWwindow* window)
{
    struct Vk_gfx_instance
    {
        VkInstance instance;
#ifndef NDEBUG
        VkDebugUtilsMessengerEXT debug_utils_messenger;
#endif
        VkSurfaceKHR surface;
    } gfx_inst;

    VkResult err;

    // Build vulkan instance (targeting Vulkan 1.3).
    vkb::InstanceBuilder builder;
    vkb::Result<vkb::Instance> instance_build_result{
        builder
            .set_app_name("Hawsoo Monolithic Renderer")
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
        return;
    }

    {
        vkb::Instance instance{ instance_build_result.value() };
        gfx_inst.instance = instance.instance;
#ifndef NDEBUG
        gfx_inst.debug_utils_messenger = instance.debug_messenger;
#endif
    }

    // Build presentation surface.
    err = glfwCreateWindowSurface(gfx_inst.instance, window, nullptr, &gfx_inst.surface);
    if (err)
    {
        throw std::runtime_error("Vulkan surface creation failed.");
        return;
    }

    // @TODO: vulkan feature selection etc.
}

}  // namespace


void TXP::GFX::setup_renderer(std::string const& title, int32_t width, int32_t height)
{
    init_glfw_no_api();
    apply_window_props(width, height);
    auto window = create_window(title, width, height);
    init_vulkan(window);
}

#endif // TXP_GFX_BACKEND_VULKAN
