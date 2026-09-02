#pragma once

#include "btglm.h"
#include "renderer/types.h"
#include "txp_renderer/camera/camera.h"
#include "txp_renderer/input_handler/input_handler.h"

#include <vector>


namespace TXP
{

/// Camera_internal service.
class Camera_internal
{
public:
    Camera_internal();

    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);

    static constexpr size_t k_controlling_camera_state_none{ (size_t)-1 };

    void set_controlling_camera(size_t camera_idx);
    size_t get_controlling_camera() const;

    void update(float_t delta_time);

    void calc_cam_matrices();
    std::vector<Cam_matrix> const& get_calcd_cam_matrices() const;

    void get_main_cam_position(vec3 out_cam_position) const;
    void get_main_cam_view_direction(vec3 out_cam_view_direction) const;
    bool is_main_cam_follow_orbit() const;

    void set_main_cam_follow_orbit_cam_offset_pos(vec3 const offset_position);
    void get_main_cam_follow_orbit_cam_offset_pos(vec3 out_offset_position) const;
    void set_main_cam_follow_orbit_follow_pos(vec3 const follow_position);
    void get_main_cam_follow_orbit_follow_pos(vec3 out_follow_position) const;
    void set_main_cam_follow_orbit_orbits(vec2 const orbit_angles);

private:
    Input::Input_handler& m_input_handler;
    Input::Cursor_pos_state m_prev_cursor_state;

    float_t m_look_sensitivity{ 0.1f };
    float_t m_fly_speed{ 20.0f };

    void update_fly_cam(vec2 look_delta_raw, float_t delta_time);

    float_t m_orbit_sensitivity_x{ 0.01875f * 0.25f * 0.25f };
    float_t m_orbit_sensitivity_y{ 0.0125f * 0.25f * 0.25f };
    float_t m_max_orbit_y{ glm_rad(89.0f) };

    vec2 m_orbits{ 0, 0 };
    vec3 m_orbit_cam_offset_position{ 0, 0, -2 };
    vec3 m_orbit_follow_position = GLM_VEC3_ZERO_INIT;

    void update_orbit_cam(vec2 look_delta_raw);

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

    std::vector<Cam_matrix> m_camera_matrices;

    size_t m_controlling_camera_state_idx{ k_controlling_camera_state_none };
    int32_t m_ignore_mouse_delta_frames{ 0 };
};

}  // namespace TXP
