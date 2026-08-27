#pragma once

#include <cmath>
#include <cstdint>
#include <memory>


namespace TXP
{
namespace Input
{

/// Modifiers for keyboard and mouse events.
struct Modifier_bits
{
    int32_t modbits{ 0 };

    // Mod bits funcs.
    bool has_shift();  // @TODO: @THEA: there needs to be a way to have exclusive modbits checking!!!!
    bool has_control();
    bool has_alt();
    bool has_super();
    bool has_capslock();
    bool has_numlock();
};

/// State of a keyboard key.
struct Keyboard_key_state
{
    bool pressed{ false };
    bool repeat{ false };
    Modifier_bits modbits;
    size_t last_event_tick{ 0 };
};

/// State of a mouse button.
struct Mouse_button_state
{
    bool pressed{ false };
    Modifier_bits modbits;
    size_t last_event_tick{ 0 };
};

/// State of the cursor.
struct Cursor_pos_state
{
    double_t xpos{ 0.0 };
    double_t ypos{ 0.0 };
    bool valid{ false };
};

/// State of scrolling.
struct Scroll_state
{
    double_t xoffset{ 0.0 };
    double_t yoffset{ 0.0 };
    size_t last_event_tick{ 0 };
};

/// State of a window.
struct Window_state
{
    bool focused{ false };
    bool iconified{ false };
    int32_t size[2]{ -1, -1 };
    size_t last_event_tick{ 0 };
};


/// Service for handling input from the input context (i.e. the window in desktop OSs).
class Input_handler
{
public:
    Input_handler();
    ~Input_handler();

    void keyboard_event(int32_t key, bool pressed, bool repeat, int32_t modbits);
    void mouse_button_event(int32_t button, bool pressed, int32_t modbits);
    void cursor_position_event(double_t xpos, double_t ypos);
    void scroll_event(double_t xoffset, double_t yoffset);
    void gamepad_connect_event(int32_t jid, bool connected);
    void window_focus_event(bool focused);
    void window_iconify_event(bool iconified);
    void window_resize_event(int32_t width, int32_t height);

    Keyboard_key_state const& get_keyboard_key_state(int32_t key) const;
    Mouse_button_state const& get_mouse_button_state(int32_t button) const;
    Cursor_pos_state const& get_cursor_pos_state() const;
    Scroll_state const& get_scroll_state() const;
    Window_state const& get_window_state() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Input
}  // namespace TXP
