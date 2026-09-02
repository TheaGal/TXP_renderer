#include "camera_internal.h"

#include "btdatecheck.h"
#include "btglm.h"
#include "btlogger.h"
#include "btservice_finder.h"
#include "renderer/types.h"
#include "txp_renderer/input_handler/input_handler.h"
#include "txp_renderer/input_handler/input_key_codes.h"

#include <cassert>
#include <stdexcept>
#include <vector>


namespace TXP
{

Camera_internal::Camera_internal()
    : m_input_handler(BT::service_finder::find_service<Input::Input_handler>())
{
    BT_SERVICE_FINDER_ADD_SERVICE(Camera_internal, this);
}

void Camera_internal::set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes)
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

void Camera_internal::set_controlling_camera(size_t camera_idx)
{
    m_controlling_camera_state_idx = camera_idx;
    m_ignore_mouse_delta_frames = 1;
}

size_t Camera_internal::get_controlling_camera() const
{
    return m_controlling_camera_state_idx;
}

void Camera_internal::update(float_t delta_time)
{
    if (m_controlling_camera_state_idx >= m_camera_states.size())
    {
        m_controlling_camera_state_idx = k_controlling_camera_state_none;  // In case dangling, reset.
        return;  // Exit early since camera doesn't exist.
    }

    // Calc look delta.
    vec2 look_delta_raw;
    {
        auto cursor_state{ m_input_handler.get_cursor_pos_state() };
        Input::Cursor_pos_state cursor_delta{ cursor_state.xpos - m_prev_cursor_state.xpos,
                                              cursor_state.ypos - m_prev_cursor_state.ypos };

        if (!m_prev_cursor_state.valid)
        {   // Clear delta.
            cursor_delta.xpos = 0;
            cursor_delta.ypos = 0;
        }

        if (m_ignore_mouse_delta_frames > 0)
        {
            m_ignore_mouse_delta_frames--;

            // Clear delta.
            cursor_delta.xpos = 0;
            cursor_delta.ypos = 0;
        }

        look_delta_raw[0] = cursor_delta.xpos;
        look_delta_raw[1] = cursor_delta.ypos;

        // Update previous state.
        m_prev_cursor_state = cursor_state;
    }

    // Tick camera controls.
    if (m_controlling_camera_state_idx == 0)
        update_orbit_cam(look_delta_raw);
    else
        update_fly_cam(look_delta_raw, delta_time);
}

void Camera_internal::calc_cam_matrices()
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

    m_camera_matrices = std::move(cam_matrices);
}

std::vector<Cam_matrix> const& Camera_internal::get_calcd_cam_matrices() const
{
    return m_camera_matrices;
}

void Camera_internal::get_main_cam_position(vec3 out_position) const
{
    if (m_camera_states.empty())
    {
        BT_WARN(
            "No camera states, just going to use zero vector as default. Fix this if you feel "
            "like it.");
        glm_vec3_zero(out_position);
        return;
    }

    BT::date_deadline(2026, 10, 24);  // @TODO: in case if there's a world-streaming or chunking system, figure out more better way of going from real to float.
    out_position[0] = m_camera_states.front().position.x;
    out_position[1] = m_camera_states.front().position.y;
    out_position[2] = m_camera_states.front().position.z;
}

void Camera_internal::get_main_cam_view_direction(vec3 out_cam_view_direction) const
{
    if (m_camera_states.empty())
    {
        BT_WARN(
            "No camera states, just going to use Z-forward vector as default. Fix this if you feel "
            "like it.");
        glm_vec3_copy(vec3{ 0, 0, 1 }, out_cam_view_direction);
        return;
    }

    glm_vec3_copy(const_cast<float_t*>(m_camera_states.front().view_direction.raw),
                  out_cam_view_direction);
}

bool Camera_internal::is_main_cam_follow_orbit() const
{
    return (get_controlling_camera() == 0);
}

void Camera_internal::set_main_cam_follow_orbit_cam_offset_pos(vec3 const offset_position)
{
    glm_vec3_copy(const_cast<float_t*>(offset_position), m_orbit_cam_offset_position);
}

void Camera_internal::get_main_cam_follow_orbit_cam_offset_pos(vec3 out_offset_position) const
{
    glm_vec3_copy(const_cast<float_t*>(m_orbit_cam_offset_position), out_offset_position);
}

void Camera_internal::set_main_cam_follow_orbit_follow_pos(vec3 const follow_position)
{
    glm_vec3_copy(const_cast<float_t*>(follow_position), m_orbit_follow_position);
}

void Camera_internal::get_main_cam_follow_orbit_follow_pos(vec3 out_position) const
{
    glm_vec3_copy(const_cast<float_t*>(m_orbit_follow_position), out_position);
}

void Camera_internal::set_main_cam_follow_orbit_orbits(vec2 const orbit_angles)
{
    glm_vec2_copy(const_cast<float_t*>(orbit_angles), m_orbits);
}

void Camera_internal::update_fly_cam(vec2 look_delta_raw, float_t delta_time)
{
    auto& camera{ m_camera_states[m_controlling_camera_state_idx] };

    // Get input.
    auto key_w_state{ m_input_handler.get_keyboard_key_state(BT_KEY_W) };
    auto key_a_state{ m_input_handler.get_keyboard_key_state(BT_KEY_A) };
    auto key_s_state{ m_input_handler.get_keyboard_key_state(BT_KEY_S) };
    auto key_d_state{ m_input_handler.get_keyboard_key_state(BT_KEY_D) };
    auto key_e_state{ m_input_handler.get_keyboard_key_state(BT_KEY_E) };
    auto key_q_state{ m_input_handler.get_keyboard_key_state(BT_KEY_Q) };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Move camera with camera delta.
    vec2 cooked_cam_delta;
    glm_vec2_scale(look_delta_raw, m_look_sensitivity, cooked_cam_delta);

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

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Move camera position with keys.
    vec2 cooked_mvt;
    glm_vec2_scale(
        vec2{ key_d_state.pressed == key_a_state.pressed ? 0.0f
                                                         : (key_d_state.pressed ? 1.0f : -1.0f),
              key_w_state.pressed == key_s_state.pressed ? 0.0f
                                                         : (key_w_state.pressed ? 1.0f : -1.0f) },
        m_fly_speed * delta_time,
        cooked_mvt);

    vec3 cam_delta_f;
    glm_vec3_scale(camera.view_direction.raw, cooked_mvt[1], cam_delta_f);
    rvec3 cam_delta_r;
    cam_delta_r[0] = cam_delta_f[0];
    cam_delta_r[1] = cam_delta_f[1];
    cam_delta_r[2] = cam_delta_f[2];
    btglm_rvec3_add(camera.position.raw, cam_delta_r, camera.position.raw);

    glm_vec3_scale(facing_direction_right, cooked_mvt[0], cam_delta_f);
    cam_delta_r[0] = cam_delta_f[0];
    cam_delta_r[1] = cam_delta_f[1];
    cam_delta_r[2] = cam_delta_f[2];
    btglm_rvec3_add(camera.position.raw, cam_delta_r, camera.position.raw);

    // Moving camera along Y-axis directly.
    camera.position.y +=
        (key_e_state.pressed == key_q_state.pressed ? 0.0f : (key_e_state.pressed ? 1.0f : -1.0f)) *
        m_fly_speed * delta_time;
}

void Camera_internal::update_orbit_cam(vec2 look_delta_raw)
{
    auto& camera{ m_camera_states[m_controlling_camera_state_idx] };

    // Set new orbit values.
    float_t look_delta_x{ look_delta_raw[0] * m_orbit_sensitivity_x };
    float_t look_delta_y{ look_delta_raw[1] * m_orbit_sensitivity_y };

    m_orbits[0] -= look_delta_x;
    while (m_orbits[0] >= glm_rad(360.0f))
        m_orbits[0] -= glm_rad(360.0f);
    while (m_orbits[0] < 0.0f)
        m_orbits[0] += glm_rad(360.0f);

    m_orbits[1] += look_delta_y;
    m_orbits[1] = glm_clamp(m_orbits[1], -m_max_orbit_y, m_max_orbit_y);

    // Calculate look offset.
    vec3 offset_from_follow_obj;
    glm_vec3_copy(m_orbit_cam_offset_position, offset_from_follow_obj);

    mat4 look_rotation;
    glm_euler_zyx(vec3{ m_orbits[1], m_orbits[0], 0.0f }, look_rotation);
    glm_mat4_mulv3(look_rotation, offset_from_follow_obj, 0.0f, offset_from_follow_obj);

    // Position camera transform.
    glm_vec3_add(m_orbit_follow_position,
                 vec3{ 0.0f, m_orbit_follow_offset_y, 0.0f },
                 camera.position.raw);
    glm_vec3_add(camera.position.raw, offset_from_follow_obj, camera.position.raw);

    glm_vec3_negate_to(offset_from_follow_obj, camera.view_direction.raw);
    glm_vec3_normalize(camera.view_direction.raw);
}

}  // namespace TXP
