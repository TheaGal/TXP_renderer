#pragma once

#include "btglm.h"

#include <memory>


namespace TXP
{

/// Projection and view matrices for a camera.
struct Cam_matrix
{
    mat4 projection;
    mat4 view;
};

/// Externally-facing camera class.
class Camera
{
public:
    Camera();
    ~Camera();

    void get_position(vec3 out_position) const;
    void get_view_direction(vec3 out_direction) const;

    std::vector<Cam_matrix> const& get_calculated_camera_matrices() const;

    bool is_follow_orbit() const;
    void set_follow_orbit_cam_offset_pos(vec3 const offset_position);
    void get_follow_orbit_cam_offset_pos(vec3 out_offset_position) const;
    void set_follow_orbit_follow_pos(vec3 const position);
    void get_follow_orbit_follow_pos(vec3 out_position) const;
    void set_follow_orbit_orbits(vec2 const orbit_angles);

    bool is_cursor_free() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
