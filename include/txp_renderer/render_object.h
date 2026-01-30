#pragma once

#include "cglm/mat4.h"

#include <cstddef>
#include <cstdint>


namespace TXP
{

/// Bitmask for filtering layers to render.
enum Render_layer : uint16_t
{
    RENDER_LAYER_ALL          = 0b1111'1111'1111'1111,
    RENDER_LAYER_NONE         = 0b0000'0000'0000'0000,

    RENDER_LAYER_DEFAULT      = 0b0000'0000'0000'0001,
    RENDER_LAYER_INVISIBLE    = 0b0000'0000'0000'0010,
    RENDER_LAYER_LEVEL_EDITOR = 0b0000'0000'0000'0100,
};

/// Lightweight object with properties on what to render in the render-object stage.
struct Render_object
{
    Render_layer layer{ RENDER_LAYER_DEFAULT };

    uint16_t render_model_idx;
    uint16_t material_set_idx;  // Default: pulls from render model material set.
    uint16_t animator_idx{ (uint16_t)-1 };  // opt: -1 means no animator.

    size_t padding0;

    mat4 transform = GLM_MAT4_IDENTITY_INIT;
};

}  // namespace TXP
