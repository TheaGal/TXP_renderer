#pragma once

#include <cmath>
#include <cstdint>


namespace TXP
{
namespace Input
{

class Input_handler
{
public:
    void keyboard_event(int32_t key, bool pressed, bool repeat);
    void mouse_button_event(int32_t button, bool pressed);
    void cursor_position_event(double_t xpos, double_t ypos);
    void scroll_event(double_t xoffset, double_t yoffset);
    void window_focus_event(bool focused);
    void window_iconify_event(bool iconified);
    void window_resize_event(int32_t width, int32_t height);
};

}  // namespace Input
}  // namespace TXP
