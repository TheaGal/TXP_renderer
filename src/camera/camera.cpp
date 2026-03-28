#include "txp_renderer/camera/camera.h"
#include "btdatecheck.h"
#include <memory>


namespace TXP
{

// struct Impl.
struct Camera::Impl
{

};


// class Camera.
Camera::Camera()
    : m_pimpl(std::make_unique<Impl>())
{
}

Camera::~Camera() = default;

void Camera::get_position(vec3 out_position) const
{
    BT::date_deadline(2026, 4, 24);  // @TODO: implement.
    glm_vec3_zero(out_position);
}

void Camera::get_view_direction(vec3 out_direction) const
{
    BT::date_deadline(2026, 4, 24);  // @TODO: implement.
    glm_vec3_zero(out_direction);
}

bool Camera::is_follow_orbit() const
{
    BT::date_deadline(2026, 4, 24);  // @TODO: implement.
    return false;
}

void Camera::get_follow_orbit_follow_pos(vec3 out_position) const
{
    BT::date_deadline(2026, 4, 24);  // @TODO: implement.
    glm_vec3_zero(out_position);
}

void Camera::set_follow_orbit_orbits(vec2 orbit_angles)
{
    BT::date_deadline(2026, 4, 24);  // @TODO: implement.
}

}  // namespace TXP
