#pragma once

#include "renderer/gfx.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

struct Material_organizer;  // Forward decl.
struct Render_object;  // Forward decl.
struct Render_object_model_mesh_reference;  // Forward decl.

namespace Shader
{

/// Postprocess compute shader.
class Shader_postprocess
{
public:
    static constexpr char const* k_name{ "postprocess" };

    Shader_postprocess(void* graphics);
    ~Shader_postprocess();

    void signal_render_view_sizes_changed();

    void compute(bool use_ui_image, void* render_view_param);

    void wait_until_completion();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
