#pragma once

#include <string>


namespace TXP
{
namespace GFX
{

/// Sets up the renderer platform-dependent backend and the render graph.
void setup_renderer(std::string const& title, int32_t width, int32_t height);

/// Tears down renderer, cleaning up resources.
void teardown_renderer();

/// Acquires next render image. Will block until an image becomes available.
uint32_t acquire_next_image();

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

}  // namespace GFX
}  // namespace TXP
