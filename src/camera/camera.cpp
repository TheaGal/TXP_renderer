#include "txp_renderer/camera/camera.h"

#include "btservice_finder.h"
#include "camera/camera_internal.h"

#include <memory>


namespace TXP
{

// struct Impl.
struct Camera::Impl
{
    Camera_internal& camera;
};


// class Camera.
Camera::Camera()
    : m_pimpl(std::make_unique<Impl>(BT::service_finder::find_service<Camera_internal>()))
{
}

Camera::~Camera() = default;

float_t Camera::get_aspect_ratio() const
{
    return m_pimpl->camera.get_main_cam_aspect();
}

void Camera::get_position(vec3 out_position) const
{
    m_pimpl->camera.get_main_cam_position(out_position);
}

void Camera::get_view_direction(vec3 out_direction) const
{
    m_pimpl->camera.get_main_cam_view_direction(out_direction);
}

std::vector<Cam_matrix> const& Camera::get_calculated_camera_matrices() const
{
    return m_pimpl->camera.get_calcd_cam_matrices();
}

bool Camera::calc_world_space_to_ndc_space(vec3 const world_position, vec3 ndc_position) const
{
    // Exit early if behind camera.
    {
        vec3 cam_position;
        m_pimpl->camera.get_main_cam_position(cam_position);

        vec3 cam_view_dir;
        m_pimpl->camera.get_main_cam_view_direction(cam_view_dir);

        vec3 obj_space_ws_pos_n;
        glm_vec3_sub(const_cast<float_t*>(world_position), cam_position, obj_space_ws_pos_n);
        glm_vec3_normalize(obj_space_ws_pos_n);

        if (glm_vec3_dot(cam_view_dir, obj_space_ws_pos_n) <= 0)
            return false;
    }

    // Pass point thru proj/view matrices.
    auto const& main_cam_matrix{ m_pimpl->camera.get_calcd_cam_matrices().front() };

    mat4 projview;
    glm_mat4_mul(const_cast<vec4*>(main_cam_matrix.projection),
                 const_cast<vec4*>(main_cam_matrix.view),
                 projview);

    vec4 my_position;
    glm_vec4(const_cast<float_t*>(world_position), 1, my_position);
    glm_mat4_mulv(projview, my_position, my_position);
    glm_vec3_scale(my_position, 1.0f / my_position[3], ndc_position);

    return true;
}

bool Camera::is_follow_orbit() const
{
    return m_pimpl->camera.is_main_cam_follow_orbit();
}

void Camera::set_follow_orbit_cam_offset_distance(float_t const offset_distance)
{
    m_pimpl->camera.set_main_cam_follow_orbit_cam_offset_distance(offset_distance);
}

float_t Camera::get_follow_orbit_cam_offset_distance() const
{
    return m_pimpl->camera.get_main_cam_follow_orbit_cam_offset_distance();
}

void Camera::set_follow_orbit_follow_pos(vec3 const position)
{
    m_pimpl->camera.set_main_cam_follow_orbit_follow_pos(position);
}

void Camera::get_follow_orbit_follow_pos(vec3 out_position) const
{
    m_pimpl->camera.get_main_cam_follow_orbit_follow_pos(out_position);
}

void Camera::set_follow_orbit_orbits(vec2 const orbit_angles)
{
    m_pimpl->camera.set_main_cam_follow_orbit_orbits(orbit_angles);
}

void Camera::set_follow_orbit_cam_angle_offset_euler(vec3 const offset_angles)
{
    m_pimpl->camera.set_main_cam_follow_orbit_cam_angle_offset_euler(offset_angles);
}

bool Camera::is_cursor_free() const
{
    return (m_pimpl->camera.get_controlling_camera() ==
            Camera_internal::k_controlling_camera_state_none);
}

}  // namespace TXP
