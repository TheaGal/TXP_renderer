#pragma once

#include "btglm.h"
#include "types.h"

#include <optional>
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
    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);

    void update();

    struct Cam_matrix
    {
        mat4 projection;
        mat4 view;
    };

    std::optional<Cam_matrix> calc_main_cam_matrix() const;

    std::vector<Cam_matrix> calc_editor_cam_matrices() const;

private:
    struct Editor_cam_state
    {
        rvec3 position;
        vec3 facing_direction;
    };
    std::vector<Editor_cam_state> m_editor_cams;

    Input::Input_handler* m_input_handler{ nullptr };
};

}  // namespace TXP
