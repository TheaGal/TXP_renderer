#pragma once

#include <memory>


namespace TXP
{
namespace Shader
{

/// Gradient compute shader.
class Shader_gradient
{
public:
    Shader_gradient(void* graphics);
    ~Shader_gradient();

    void compute(void* render_frame);

private:
    static constexpr char const* k_name{ "gradient" };

    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
