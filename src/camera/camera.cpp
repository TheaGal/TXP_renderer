#include "camera.h"

#include "btglm.h"
#include "btservice_finder.h"
#include "input_handler/input_handler.h"
#include "types.h"

#include <stdexcept>
#include <vector>


namespace TXP
{

Camera::Camera()
    : m_input_handler(BT::service_finder::find_service<Input::Input_handler>())
{
    BT_SERVICE_FINDER_ADD_SERVICE(Camera, this);
}

void Camera::set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes)
{
    size_t prev_size{ m_camera_states.size() };

    m_camera_states.resize(rend_view_sizes.size());
    for (size_t i = 0; i < m_camera_states.size(); i++)
    {
        auto& cam_state{ m_camera_states[i] };
        auto const& rend_view_size{ rend_view_sizes[i] };

        if (prev_size < i + 1)
        {   // New camera state needs to be initialized.
            if (i == 0)
            {   // Default for main state.
                cam_state.position = { 0.0, 1.0, -10.0 };
                cam_state.view_direction = { 0.0f, 0.0f, 1.0f };
                cam_state.is_ortho = false;
                cam_state.ortho_size = 10.0f;
                cam_state.z_near = 0.1f;
                cam_state.z_far = 1000.0f;
                cam_state.fov = glm_rad(90.0f);
            }
            else
            {   // Copy from previous valid state.
                size_t copy_idx{ prev_size == 0 ? 0 : prev_size - 1 };
                cam_state = m_camera_states[copy_idx];
            }
        }

        cam_state.aspect = (static_cast<float_t>(rend_view_size.width) /
                            static_cast<float_t>(rend_view_size.height));
    }
}

void Camera::update()
{
    // @TODO: use input to control camera.
    // assert(false);  // @THEA: @NOCHECKIN: revert this commented out and replace it with camera controls asap.
}

std::vector<Camera::Cam_matrix> Camera::calc_cam_matrices() const
{
    std::vector<Cam_matrix> cam_matrices;
    cam_matrices.reserve(m_camera_states.size());

    for (auto const& cam : m_camera_states)
    {
        Cam_matrix new_cam_matrix;

        if (cam.is_ortho)
        {   // Calculate orthographic projection matrix.
            glm_ortho(-cam.ortho_size * cam.aspect,
                      cam.ortho_size * cam.aspect,
                      -cam.ortho_size,
                      cam.ortho_size,
                      cam.z_near,
                      cam.z_far,
                      new_cam_matrix.projection);
            new_cam_matrix.projection[1][1] *= -1.0f;  // Fix neg-Y issue.
        }
        else
        {   // Calculate projection matrix.
            glm_perspective(cam.fov,
                            cam.aspect,
                            cam.z_near,
                            cam.z_far,
                            new_cam_matrix.projection);
            new_cam_matrix.projection[1][1] *= -1.0f;  // Fix neg-Y issue.
        }

        // Calculate view matrix.
        vec3 up{ 0.0f, 1.0f, 0.0f };
        if (std::abs(cam.view_direction.x) < 1e-6f &&
            std::abs(cam.view_direction.y) > 0.5f &&
            std::abs(cam.view_direction.z) < 1e-6f)
        {
            glm_vec3_copy(vec3{ 0.0f, 0.0f, 1.0f }, up);
        }

        vec3 cam_position{  // @TODO: figure out how to convert real to float here!!
            cam.position.x,
            cam.position.y,
            cam.position.z,
        };
        if (glm_vec3_max(cam_position) > 2500.0f ||
            glm_vec3_min(cam_position) < -2500.0f)  // @NOTE: this is a forced crash to ensure that I work on this issue someday in the future.  -Thea 2026/03/03
            throw std::runtime_error(
                "@THEA: you need to figure out origin shifting for the camera position. It's "
                "degraded too much at this point.");

        vec3 center;
        glm_vec3_add(cam_position, const_cast<float_t*>(cam.view_direction.raw), center);
        glm_lookat(cam_position, center, up, new_cam_matrix.view);

        cam_matrices.emplace_back(std::move(new_cam_matrix));
    }

    return cam_matrices;
}

}  // namespace TXP
