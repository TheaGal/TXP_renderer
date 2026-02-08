#pragma once

#include "txp_renderer/types.h"

#include <memory>
#include <string>
#include <vector>


namespace TXP
{

/// Renderer backend with implementation depending on the platform using preprocessor macros in the
/// .cpp source file to differentiate the different implementations.
class Graphics
{
public:
    Graphics(std::string const& title, int32_t width, int32_t height);
    ~Graphics();

    /// Loads all registered assets.
    void load_assets(std::vector<Texture_asset_create_info>&& texture_assets,
                     std::vector<Material_asset_create_info>&& material_assets,
                     std::vector<Material_set_asset_create_info>&& material_set_assets,
                     std::vector<Model_asset_create_info>&& model_assets);

    /// Polls for input events.
    void poll_input_events();

    /// Builds one imgui frame using a callback and readies the frame for rendering.
    void build_imgui_frame();

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

    /// Renders collected immediate-mode GUI commands to LDR present surface.
    void render_imgui();

    /// Presents rendered frame to screen.
    void present_frame_to_screen();

    /// Waits until GPU device is idle.
    void wait_until_gpu_idle();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
