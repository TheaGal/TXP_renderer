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
    Shader_gradient(void* renderer);
    ~Shader_gradient();

    void compute(void* param);

private:
    static constexpr char const* k_fname{ "gradient.shader" };

    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
