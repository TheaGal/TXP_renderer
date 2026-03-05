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

    std::unordered_map<int32_t, Keyboard_key_state> keyboard_key_states_map;

    std::unordered_map<int32_t, Mouse_button_state> mouse_button_states_map;

    Cursor_pos_state cursor_pos_state;

    Scroll_state scroll_state;

    std::unordered_map<int32_t, std::string> connected_gamepads_jid_to_name_map;

    Window_state window_state;
};


// Mod bits funcs.
bool Modifier_bits::has_shift()
{
    return static_cast<bool>(modbits & GLFW_MOD_SHIFT);
}
bool Modifier_bits::has_control()
{
    return static_cast<bool>(modbits & GLFW_MOD_CONTROL);
}
bool Modifier_bits::has_alt()
{
    return static_cast<bool>(modbits & GLFW_MOD_ALT);
}
bool Modifier_bits::has_super()
{
    return static_cast<bool>(modbits & GLFW_MOD_SUPER);
}
bool Modifier_bits::has_capslock()
{
    return static_cast<bool>(modbits & GLFW_MOD_CAPS_LOCK);
}
bool Modifier_bits::has_numlock()
{
    return static_cast<bool>(modbits & GLFW_MOD_NUM_LOCK);
}


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
        .valid = true,
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

Keyboard_key_state const& Input_handler::get_keyboard_key_state(int32_t key) const
{
    return m_pimpl->keyboard_key_states_map[key];
}

Mouse_button_state const& Input_handler::get_mouse_button_state(int32_t button) const
{
    return m_pimpl->mouse_button_states_map[button];
}

Cursor_pos_state const& Input_handler::get_cursor_pos_state() const
{
    return m_pimpl->cursor_pos_state;
}

Scroll_state const& Input_handler::get_scroll_state() const
{
    return m_pimpl->scroll_state;
}

Window_state const& Input_handler::get_window_state() const
{
    return m_pimpl->window_state;
}

}  // namespace Input
}  // namespace TXP
