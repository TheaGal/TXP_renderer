#pragma once

#include "btglm.h"

#include <memory>


namespace TXP
{

/// Externally-facing camera class.
class Camera
{
public:
    Camera();
    ~Camera();

    void get_position(vec3 out_position) const;
    void get_view_direction(vec3 out_direction) const;

    bool is_follow_orbit() const;
    void get_follow_orbit_follow_pos(vec3 out_position) const;
    void set_follow_orbit_orbits(vec2 orbit_angles);

    bool is_cursor_free() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
