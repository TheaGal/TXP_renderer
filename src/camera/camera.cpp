#include "txp_renderer/camera/camera.h"
#include "btdatecheck.h"
#include <memory>


namespace TXP
{

// struct Impl.
struct Camera::Impl
{

};


// class Camera.
Camera::Camera()
    : m_pimpl(std::make_unique<Impl>())
{
    BT::date_deadline(2026, 4, 24);  // @TODO: add camera ifc.
}

Camera::~Camera() = default;

}  // namespace TXP
