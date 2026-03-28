#pragma once

#include "btglm.h"
#include "renderer/types.h"
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

    struct Cam_matrix
    {
        mat4 projection;
        mat4 view;
    };

    std::vector<Cam_matrix> calc_cam_matrices() const;

    void get_main_cam_position(vec3 out_cam_position) const;
    void get_main_cam_view_direction(vec3 out_cam_view_direction) const;
    bool is_main_cam_follow_orbit() const;
    void get_main_cam_follow_orbit_follow_pos(vec3 out_follow_position) const;
    void set_main_cam_follow_orbit_orbits(vec2 orbit_angles);

private:
    Input::Input_handler& m_input_handler;
    Input::Cursor_pos_state m_prev_cursor_state;

    float_t m_look_sensitivity{ 0.1f };
    float_t m_fly_speed{ 20.0f };

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

    size_t m_controlling_camera_state_idx{ k_controlling_camera_state_none };
    int32_t m_ignore_mouse_delta_frames{ 0 };
};

}  // namespace TXP
