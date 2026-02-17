#if TXP_GFX_BACKEND_VULKAN

#include "shader_gradient.h"

#include "renderer/gfx_vulkan_impl.h"

#include <memory>


namespace TXP
{
namespace Shader
{

// struct Shader_gradient::Impl
struct Shader_gradient::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , hdr_draw_image_color(g.hdr_draw_image_color.get_image())
    {
    }

    TXP::Graphics::Impl& g;

    Vk_Image::Image& hdr_draw_image_color;
};


// class Shader_gradient
Shader_gradient::Shader_gradient(void* graphics)
    : m_pimpl(std::make_unique<Impl>(*static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_gradient::~Shader_gradient() = default;

void Shader_gradient::compute(void* param)
{
    auto& p{ *m_pimpl };

    // @TODO: create a get-current-frame func.
    auto cmd{
        p.g.frames[p.g.current_frame_idx % p.g.k_frame_overlap].graphics_queue_command_buffer.get()
    };

    Vk_Image::Image::transition_to(
        cmd,
        { { &p.hdr_draw_image_color, VK_IMAGE_LAYOUT_GENERAL } });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.g.draw_image_compute_pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.g.draw_image_compute_pipeline_layout,
                            0,
                            1, &p.g.draw_image_descriptors,
                            0, nullptr);

    // "threadGroupSize": [16, 16, 1],
    vkCmdDispatch(cmd, std::ceil(1280.0 / 16.0), std::ceil(720.0 / 16.0), 1);
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
