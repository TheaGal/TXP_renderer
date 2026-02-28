#include "input_handler.h"

#include "btservice_finder.h"

#include <GLFW/glfw3.h>
#include <unordered_map>


namespace TXP
{
namespace Input
{

// struct Impl
struct Input_handler::Impl
{
    size_t event_tick{ 0 };

    struct Modifier_bits
    {
        int32_t modbits{ 0 };

        // Mod bits funcs.
        bool has_shift()
        {
            return static_cast<bool>(modbits & GLFW_MOD_SHIFT);
        }
        bool has_control()
        {
            return static_cast<bool>(modbits & GLFW_MOD_CONTROL);
        }
        bool has_alt()
        {
            return static_cast<bool>(modbits & GLFW_MOD_ALT);
        }
        bool has_super()
        {
            return static_cast<bool>(modbits & GLFW_MOD_SUPER);
        }
        bool has_capslock()
        {
            return static_cast<bool>(modbits & GLFW_MOD_CAPS_LOCK);
        }
        bool has_numlock()
        {
            return static_cast<bool>(modbits & GLFW_MOD_NUM_LOCK);
        }
    };

    struct Keyboard_key_state
    {
        bool pressed{ false };
        bool repeat{ false };
        Modifier_bits modbits;
        size_t last_event_tick{ 0 };
    };

    std::unordered_map<int32_t, Keyboard_key_state> keyboard_key_states_map;

    struct Mouse_button_state
    {
        bool pressed{ false };
        Modifier_bits modbits;
        size_t last_event_tick{ 0 };
    };

    std::unordered_map<int32_t, Mouse_button_state> mouse_button_states_map;

    struct Cursor_pos_state
    {
        double_t xpos{ 0.0 };
        double_t ypos{ 0.0 };
    } cursor_pos_state;

    struct Scroll_state
    {
        double_t xoffset{ 0.0 };
        double_t yoffset{ 0.0 };
    } scroll_state;

    std::unordered_map<int32_t, std::string> connected_gamepads_jid_to_name_map;

    struct Window_state
    {
        bool focused{ false };
        bool iconified{ false };
        int32_t size[2]{ -1, -1 };
        size_t last_event_tick{ 0 };
    } window_state;
};


// class Input_handler
Input_handler::Input_handler()
    : m_pimpl(std::make_unique<Impl>())
{
    BT_SERVICE_FINDER_ADD_SERVICE(Input_handler, this);
}

Input_handler::~Input_handler() = default;

void Input_handler::keyboard_event(int32_t key, bool pressed, bool repeat, int32_t modbits)
{
    m_pimpl->keyboard_key_states_map[key] = {
        .pressed = pressed,
        .repeat = repeat,
        .modbits{ modbits },
        .last_event_tick = ++m_pimpl->event_tick,
    };
}

void Input_handler::mouse_button_event(int32_t button, bool pressed, int32_t modbits)
{
    m_pimpl->mouse_button_states_map[button] = {
        .pressed = pressed,
        .modbits{ modbits },
        .last_event_tick = ++m_pimpl->event_tick,
    };
}

void Input_handler::cursor_position_event(double_t xpos, double_t ypos)
{
    m_pimpl->cursor_pos_state = {
        .xpos = xpos,
        .ypos = ypos,
    };
}

void Input_handler::scroll_event(double_t xoffset, double_t yoffset)
{
    m_pimpl->scroll_state = {
        .xoffset = xoffset,
        .yoffset = yoffset,
    };
}

void Input_handler::gamepad_connect_event(int32_t jid, bool connected)
{
    // Ignore non-gamepad joysticks.
    if (!glfwJoystickIsGamepad(jid))
        return;

    if (connected)
    {
        m_pimpl->connected_gamepads_jid_to_name_map.emplace(jid, glfwGetGamepadName(jid));
    }
    else
    {
        auto& jid_map{ m_pimpl->connected_gamepads_jid_to_name_map };
        if (auto it = jid_map.find(jid); it != jid_map.end())
            jid_map.erase(it);
    }
}

void Input_handler::window_focus_event(bool focused)
{
    m_pimpl->window_state.focused = focused;
    m_pimpl->window_state.last_event_tick = ++m_pimpl->event_tick;
}

void Input_handler::window_iconify_event(bool iconified)
{
    m_pimpl->window_state.iconified = iconified;
    m_pimpl->window_state.last_event_tick = ++m_pimpl->event_tick;
}

void Input_handler::window_resize_event(int32_t width, int32_t height)
{
    m_pimpl->window_state.size[0] = width;
    m_pimpl->window_state.size[1] = height;
    m_pimpl->window_state.last_event_tick = ++m_pimpl->event_tick;
}

}  // namespace Input
}  // namespace TXP
