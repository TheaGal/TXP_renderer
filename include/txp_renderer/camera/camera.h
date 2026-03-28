#pragma once

#include <memory>


namespace TXP
{

/// Externally-facing camera class.
class Camera
{
public:
    Camera();
    ~Camera();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
