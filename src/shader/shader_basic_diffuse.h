#pragma once

#include <memory>
#include <string>
#include <unordered_map>


namespace TXP
{

// Forward decl.
struct Material_collection;

namespace Shader
{

/// Textured, diffuse shader.
class Shader_basic_diffuse
{
public:
    static constexpr char const* k_name{ "basic_diffuse" };

    Shader_basic_diffuse(Material_collection& material_collection, void* graphics);
    ~Shader_basic_diffuse();

    void make_material(std::string const& material_name,
                       std::unordered_map<std::string, std::string> const& shader_params);
    void build_material_collection();

    void draw(void* render_view_param);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
