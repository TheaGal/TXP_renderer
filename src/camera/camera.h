#pragma once

#include "btglm.h"
#include "types.h"

#include <vector>


namespace TXP
{
namespace Input
{
class Input_handler;
}  // namespace Input

/// Camera service.
class Camera
{
public:
    Camera();

    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);

    void update();

    struct Cam_matrix
    {
        mat4 projection;
        mat4 view;
    };

    std::vector<Cam_matrix> calc_cam_matrices() const;

private:
    Input::Input_handler& m_input_handler;

    struct Camera_state
    {
        rvec3s position;
        vec3s view_direction;

        bool is_ortho;
        float_t ortho_size;

        float_t z_near;
        float_t z_far;
        float_t fov;

        float_t aspect;
    };
    std::vector<Camera_state> m_camera_states;
};

}  // namespace TXP
