#pragma once

#include <cstddef>
#include <memory>


namespace TXP
{

struct Render_model_data_collection;  // Forward decl.
struct UI_state;  // Forward decl.

namespace Shader
{

/// Raymarching thru a volume field.
class Shader_clouds
{
public:
    static constexpr char const* k_name{ "clouds" };

    Shader_clouds(Render_model_data_collection& render_model_data_collection,
                  void* graphics);
    ~Shader_clouds();

    void upload_ui_state_data(UI_state const& ui_state);

    void compute(UI_state const& ui_state, float_t camera_view_aspect_ratio);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
