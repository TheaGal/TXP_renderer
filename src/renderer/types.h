#pragma once

#include "btglm.h"

#include <cstdint>


namespace TXP
{

/// Size of a render view.
struct Render_view_size
{
    int32_t width;
    int32_t height;
};

/// Projection and view matrices for a camera.
struct Cam_matrix
{
    mat4 projection;
    mat4 view;
};

}  // namespace TXP
