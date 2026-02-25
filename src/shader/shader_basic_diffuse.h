#pragma once

#include <memory>


namespace TXP
{
namespace Shader
{

/// Textured, diffuse shader.
class Shader_basic_diffuse
{
public:
    Shader_basic_diffuse(void* graphics);
    ~Shader_basic_diffuse();

    void draw(void* param);

private:
    static constexpr char const* k_name{ "basic_diffuse" };

    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
