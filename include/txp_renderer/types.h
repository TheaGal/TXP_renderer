#pragma once

#include <cstdint>
#include <string>


namespace TXP
{

/// Key to access editing render objects.
using pool_key_t = std::uint32_t;

/// Bitmask for filtering layers to render.
enum Render_layer : uint16_t
{
    RENDER_LAYER_ALL          = 0b1111'1111'1111'1111,
    RENDER_LAYER_NONE         = 0b0000'0000'0000'0000,

    RENDER_LAYER_DEFAULT      = 0b0000'0000'0000'0001,
    RENDER_LAYER_INVISIBLE    = 0b0000'0000'0000'0010,
    RENDER_LAYER_LEVEL_EDITOR = 0b0000'0000'0000'0100,
};

/// Config for creating a render object.
struct Render_obj_create_config
{
    Render_layer layer;
    std::string model_name;

    // ^^ Required ^^ / vv Optional vv

    std::string material_set;

    struct Animated_create_config
    {
        std::string animator_template;
        std::string anim_frame_action_control;
    } deform_config;
};

}  // namespace TXP
