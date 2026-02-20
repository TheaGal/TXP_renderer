#pragma once

#include "btglm.h"
#include "txp_renderer/types.h"

#include <cstddef>
#include <cstdint>


namespace TXP
{

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
