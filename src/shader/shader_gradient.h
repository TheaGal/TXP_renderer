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

/// Gradient compute shader.
class Shader_gradient
{
public:
    static constexpr char const* k_name{ "gradient" };

    Shader_gradient(Material_collection& material_collection, void* graphics);
    ~Shader_gradient();

    void make_material(std::string const& material_name,
                       std::unordered_map<std::string, std::string> const& shader_params);
    void build_material_collection();

    void compute(void* render_frame);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
