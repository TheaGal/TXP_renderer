#include "txp_renderer/renderer.h"

#include "gfx.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>


namespace TXP
{

Renderer::Renderer(std::string const& title, int32_t width, int32_t height)
    : m_title(title)
    , m_width(width)
    , m_height(height)
{
    static std::atomic_bool s_init{ false };
    bool expect_init{ false };
    if (!s_init.compare_exchange_strong(expect_init, true))
    {
        throw std::runtime_error("Only one `TXP::Renderer` may be created.");
    }
}

Renderer::~Renderer() = default;

void Renderer::run()
{   // Setup renderer.
    auto g = std::make_unique<Graphics>(m_title, m_width, m_height);

    // Render frames until shutdown flag is tripped.
    while (!m_shutdown_flag.load())
    {   // Process render object destroy requests.

        // Process render object create requests.

        // Update animator timers (renderer-profile).

        // Calculate animator joints.

        // Poll for input events.
        g->poll_input_events();

        // Build imgui for this frame.
        g->build_imgui_frame();

        // Render One Frame.
        g->start_new_frame();

        // g->compute_light_culling();
        // g->compute_shadow_culling();
        // g->compute_opaque_geometry_culling();
        // g->compute_transparent_geometry_culling();

        // g->render_shadows();
        // g->render_opaque_geometry();
        // g->render_clouds();
        // g->render_volumetric_light();
        // g->render_particles();
        // g->render_transparent_geometry();

        g->render_hdr_to_ldr_postprocessing();
        g->render_imgui();

        g->present_frame_to_screen();

        std::cout << "RENDERED ONE FRAME" << std::endl;
    }

    // Wait until GPU is idle before destruction.
    g->wait_until_gpu_idle();
}

void Renderer::shutdown_loop()
{
    m_shutdown_flag.store(true);
}

pool_key_t Renderer::create_render_obj(Render_obj_create_config&& config)
{
    return 0;
}

void Renderer::destroy_render_obj(pool_key_t key)
{}

}  // namespace TXP
