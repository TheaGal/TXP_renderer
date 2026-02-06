#pragma once

#include <memory>
#include <string>


namespace TXP
{

/// Renderer backend with implementation depending on the platform using preprocessor macros in the
/// .cpp source file to differentiate the different implementations.
class Graphics
{
public:
    Graphics(std::string const& title, int32_t width, int32_t height);
    ~Graphics();

    /// Polls for input events.
    void poll_input_events();

    /// Acquires next render image for the frame. Will block until an image becomes available.
    void start_new_frame();

    /// .
    void compute_light_culling();

    /// .
    void compute_shadow_culling();

    /// .
    void compute_opaque_geometry_culling();

    /// .
    void compute_transparent_geometry_culling();

    /// .
    void render_shadows();

    /// .
    void render_opaque_geometry();

    /// .
    void render_clouds();

    /// .
    void render_volumetric_light();

    /// .
    void render_particles();

    /// .
    void render_transparent_geometry();

    /// .
    void render_hdr_to_ldr_postprocessing();

    /// .
    void render_imgui();

    /// Presents rendered frame to screen.
    void present_frame_to_screen();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
