#pragma once

#include <cstddef>
#include <memory>


namespace TXP
{

class Camera_internal;  // Forward decl.

namespace Shader
{

/// Raymarching thru a volume field.
class Shader_clouds
{
public:
    static constexpr char const* k_name{ "clouds" };

    Shader_clouds(void* graphics);
    ~Shader_clouds();

    void signal_render_view_sizes_changed();

    void compute(Camera_internal const& camera, void* render_view_param);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
