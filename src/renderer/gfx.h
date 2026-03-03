#pragma once

#include "render_object/render_object.h"
#include "txp_renderer/types.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>


namespace TXP
{
namespace gpu_type
{

struct Environment_data
{
    mat4 projection;
    mat4 view;
    vec4 light_pos;

    static constexpr uint32_t k_lighting_mode_full        = 0;
    static constexpr uint32_t k_lighting_mode_basic_lit   = 1;
    static constexpr uint32_t k_lighting_mode_basic_unlit = 2;
    uint32_t lighting_mode;
};

struct Model_transform_set
{
    mat4 transforms[65535];
};

}  // namespace gpu_type


/// Renderer backend with implementation depending on the platform using preprocessor macros in the
/// .cpp source file to differentiate the different implementations.
class Graphics
{
public:
    Graphics(std::string const& title, int32_t width, int32_t height);
    ~Graphics();

    /// Loads all registered textures.
    void load_texture_assets(std::string const& texture_asset_dir,
                             std::vector<Texture_asset_create_info>&& texture_assets);
    void load_material_assets(std::vector<Material_asset_create_info>&& material_assets,
                              std::vector<Material_set_asset_create_info>&& material_set_assets);
    void load_model_assets(std::vector<Model_asset_create_info>&& model_assets,
                           Render_model_data_collection& render_model_data_collection);

    /// Polls for input events.
    void poll_input_events();

    /// Builds one imgui frame using a callback and readies the frame for rendering.
    void build_imgui_contents(std::vector<Render_view_size>& out_rend_view_sizes);

    /// Sets render view sizes for hdr draw images.
    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);

    /// Sets GPU camera properties for a render view.
    void set_render_view_camera(size_t render_view_idx, mat4 camera_projection, mat4 camera_view);

    /// Gets render view data for a render view.
    void* get_render_view(size_t rend_view_idx);

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

    /// Waits until GPU can use next frame resources, then starts a new frame.
    void start_next_frame();

    /// Presents rendered frame to screen.
    void present_frame_to_screen();

    /// Waits until GPU device is idle.
    void wait_until_gpu_idle();

    /// Implementation struct.
    struct Impl;

    /// Gets the implementation.
    Impl* get_impl();

private:
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
