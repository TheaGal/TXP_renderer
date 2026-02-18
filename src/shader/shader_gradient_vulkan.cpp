#include "shader_creation/shader_creation.h"
#include <stdexcept>
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
    #define WRAP_INTO_OWN_FUNC 1
    #if WRAP_INTO_OWN_FUNC
        auto refl_data = Shader_Creation::read_slang_reflection(k_name);

        if (refl_data.entryPoints.size() != 1 ||
            refl_data.entryPoints.front().stage != "compute" ||
            refl_data.entryPoints.front().threadGroupSize.size() != 3 ||
            refl_data.entryPoints.front().threadGroupSize[0] <= 0 ||
            refl_data.entryPoints.front().threadGroupSize[1] <= 0 ||
            refl_data.entryPoints.front().threadGroupSize[2] <= 0)
            std::runtime_error("Malformed shader data.");

        thread_grp_sizes[0] = refl_data.entryPoints.front().threadGroupSize[0];
        thread_grp_sizes[1] = refl_data.entryPoints.front().threadGroupSize[1];
        thread_grp_sizes[2] = refl_data.entryPoints.front().threadGroupSize[2];
    #endif // WRAP_INTO_OWN_FUNC
    }


    TXP::Graphics::Impl& g;

    Vk_Image::Image& hdr_draw_image_color;

    std::array<uint32_t, 3> thread_grp_sizes;
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

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

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

    vkCmdDispatch(cmd,
        #define WRAP_INTO_OWN_FUNC 1
        #if WRAP_INTO_OWN_FUNC
                  (1280 + p.thread_grp_sizes[0] - 1) / p.thread_grp_sizes[0],
                  (720 + p.thread_grp_sizes[1] - 1) / p.thread_grp_sizes[1],
                  (1 + p.thread_grp_sizes[2] - 1) / p.thread_grp_sizes[2]);
        #endif // WRAP_INTO_OWN_FUNC
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
