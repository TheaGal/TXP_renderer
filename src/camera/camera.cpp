#include "txp_renderer/camera/camera.h"

#include "btdatecheck.h"
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

bool Camera::is_follow_orbit() const
{
    return m_pimpl->camera.is_main_cam_follow_orbit();
}

void Camera::set_follow_orbit_follow_pos(vec3 position)
{
    m_pimpl->camera.set_main_cam_follow_orbit_follow_pos(position);
}

void Camera::get_follow_orbit_follow_pos(vec3 out_position) const
{
    m_pimpl->camera.get_main_cam_follow_orbit_follow_pos(out_position);
}

void Camera::set_follow_orbit_orbits(vec2 orbit_angles)
{
    m_pimpl->camera.set_main_cam_follow_orbit_orbits(orbit_angles);
}

bool Camera::is_cursor_free() const
{
    return (m_pimpl->camera.get_controlling_camera() ==
            Camera_internal::k_controlling_camera_state_none);
}

}  // namespace TXP
