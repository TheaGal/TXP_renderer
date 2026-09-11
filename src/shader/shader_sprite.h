#pragma once

#include <cstddef>
#include <memory>


namespace TXP
{

struct Render_model_data_collection;  // Forward decl.
struct UI_state;  // Forward decl.

namespace Shader
{

/// Unlit texture shader.
class Shader_sprite
{
public:
    static constexpr char const* k_name{ "sprite" };

    Shader_sprite(Render_model_data_collection& render_model_data_collection,
                  void* graphics);
    ~Shader_sprite();

    void upload_ui_state_data(UI_state const& ui_state);

    void draw(UI_state const& ui_state);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
