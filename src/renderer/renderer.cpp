#include "txp_renderer/renderer.h"

#include "gfx.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>


namespace TXP
{

/// Impl class for Renderer.
struct Renderer::Impl
{
    std::string title;
    int32_t width;
    int32_t height;

    std::atomic_bool shutdown_flag{ false };

    /// See `Renderer::run()`
    void run();

    /// Renders one frame to the screen using GFX.
    void render_one_frame_to_screen(uint32_t present_img_idx);
};


// struct Renderer::Impl
void Renderer::Impl::run()
{
    // Setup renderer.
    GFX::setup_renderer(title, width, height);

    while (!shutdown_flag.load())
    {   // Process render object destroy requests.

        // Process render object create requests.

        // Update animator timers (renderer-profile).

        // Calculate animator joints.

        // Wait for render opportunity.
        auto next_image_idx = GFX::acquire_next_image();

        // Render one frame.
        render_one_frame_to_screen(next_image_idx);
    }

    // Teardown renderer.
    GFX::teardown_renderer();
}

void Renderer::Impl::render_one_frame_to_screen(uint32_t present_img_idx)
{
    // @TODO.
}


// class Renderer
Renderer::Renderer(std::string const& title, int32_t width, int32_t height)
    : m_pimpl(std::make_unique<Impl>(title, width, height))
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
{
    m_pimpl->run();
}

void Renderer::shutdown_loop()
{
    m_pimpl->shutdown_flag.store(true);
}

pool_key_t Renderer::create_render_obj(Render_obj_create_config&& config)
{}

void Renderer::destroy_render_obj(pool_key_t key)
{}

}  // namespace TXP
