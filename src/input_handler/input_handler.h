#pragma once

#include <cmath>
#include <cstdint>
#include <memory>


namespace TXP
{
namespace Input
{

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

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Input
}  // namespace TXP
