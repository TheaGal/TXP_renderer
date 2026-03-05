#include "camera.h"

#include "btglm.h"
#include "btservice_finder.h"
#include "input_handler/input_handler.h"
#include "input_handler/input_key_codes.h"
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
    size_t camera_idx{ 0 };  // @HARDCODE
    if (camera_idx >= m_camera_states.size())
        return;  // Exit early since camera doesn't exist.

    auto& camera{ m_camera_states[camera_idx] };

    // Get input.
    auto w_key_state{ m_input_handler.get_keyboard_key_state(BT_KEY_W) };
    auto a_key_state{ m_input_handler.get_keyboard_key_state(BT_KEY_A) };
    auto s_key_state{ m_input_handler.get_keyboard_key_state(BT_KEY_S) };
    auto d_key_state{ m_input_handler.get_keyboard_key_state(BT_KEY_D) };
    auto cursor_state{ m_input_handler.get_cursor_pos_state() };

    // Calc delta.
    Input::Cursor_pos_state cursor_delta{ cursor_state.xpos - m_prev_cursor_state.xpos,
                                          cursor_state.ypos - m_prev_cursor_state.ypos };

    if (!m_prev_cursor_state.valid)
    {   // Clear delta.
        cursor_delta.xpos = 0;
        cursor_delta.ypos = 0;
    }

    // Move camera with camera delta.
    vec2 cooked_cam_delta;
    glm_vec2_scale(
        vec2{ static_cast<float_t>(cursor_delta.xpos), static_cast<float_t>(cursor_delta.ypos) },
        m_input_sensitivity,
        cooked_cam_delta);

    vec3 world_up{ 0.0f, 1.0f, 0.0f };
    vec3 world_down{ 0.0f, -1.0f, 0.0f };

    // Update camera view direction with input.
    vec3 facing_direction_right;
    glm_cross(camera.view_direction.raw,
              world_up,
              facing_direction_right);
    glm_normalize(facing_direction_right);

    mat4 rotation = GLM_MAT4_IDENTITY_INIT;
    glm_rotate(rotation, glm_rad(-cooked_cam_delta[1]), facing_direction_right);

    vec3 new_view_direction;
    glm_mat4_mulv3(rotation,
                   camera.view_direction.raw,
                   0.0f,
                   new_view_direction);

    if (glm_vec3_angle(new_view_direction, world_up) > glm_rad(5.0f) &&
        glm_vec3_angle(new_view_direction, world_down) > glm_rad(5.0f))
    {
        glm_vec3_copy(new_view_direction, camera.view_direction.raw);
    }

    glm_mat4_identity(rotation);
    glm_rotate(rotation, glm_rad(-cooked_cam_delta[0]), world_up);
    glm_mat4_mulv3(rotation,
                   camera.view_direction.raw,
                   0.0f,
                   camera.view_direction.raw);

    // @NOTE: Need a normalization step at the end to prevent float inaccuracy over time.
    glm_vec3_normalize(camera.view_direction.raw);








#if 0
    vec2 cooked_mvt;
    glm_vec2_scale(vec2{ input_state.move.x.val, input_state.move.y.val },
                   capture_fly.speed * delta_time,
                   cooked_mvt);

    glm_vec3_muladds(camera.view_direction,
                     cooked_mvt[1],
                     camera.position);
    glm_vec3_muladds(facing_direction_right,
                     cooked_mvt[0],
                     camera.position);

    // Update camera position with input.
    camera.position[1] +=
        input_state.le_move_world_y_axis.val * capture_fly.speed * delta_time;
#endif // 0






    // Update previous state.
    m_prev_cursor_state = cursor_state;
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
