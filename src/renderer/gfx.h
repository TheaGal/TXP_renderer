#pragma once

#include "render_object/render_object.h"
#include "renderer/types.h"
#include "txp_renderer/types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

struct Renderer_settings;  // Forward decl.
class Camera_internal;  // Forward decl.
struct Material_organizer;  // Forward decl.

namespace gpu_type
{

struct Directional_light
{
    vec4 direction_xyz_intensity_w;
    vec4 color;
};

struct Environment_data
{
    mat4 projection;
    mat4 view;
    Directional_light directional_light;

    static constexpr uint32_t k_lighting_mode_full        = 0;
    static constexpr uint32_t k_lighting_mode_basic_lit   = 1;
    static constexpr uint32_t k_lighting_mode_basic_unlit = 2;
    uint32_t lighting_mode;
};

struct Per_instance_data
{
    uint32_t model_transform_set_idx;
    uint32_t material_param_set_idx;
};

struct Per_instance_data_collection
{
    Per_instance_data per_instance_datas[65535];
};

struct Model_transform_set
{
    mat4 transforms[65535];
};

}  // namespace gpu_type


/// Information to create texture asset.
struct Texture_asset_create_info
{
    std::string texture_name;
    std::string ktx2_fname;
};

/// Information to create material asset.
struct Material_asset_create_info
{
    std::string material_name;
    std::string shader_name;
    std::unordered_map<std::string, std::string> shader_params;
};

/// Information to create material palette asset.
struct Material_palette_asset_create_info
{
    std::string mat_set_name;
    std::vector<std::string> materials;
};

/// Information to create model asset.
struct Model_asset_create_info
{
    std::string model_name;
    std::string file_ext;
    bool load_animator_template;
    bool load_anim_frame_action;
};


/// Set of information useful for graphics.
struct Information_hook_struct
{
    std::function<void(bool)> const& set_play_flag_fn;
    std::function<bool()> const& get_play_flag_fn;
    Performance_time_map_t const& perf_time_map;
};


/// Renderer backend with implementation depending on the platform using preprocessor macros in the
/// .cpp source file to differentiate the different implementations.
class Graphics
{
public:
    Graphics(std::string const& title,
             Renderer_settings& settings,
             Information_hook_struct info_hook_struct);
    ~Graphics();

    /// Sends a request flag to read the settings reference and update accordingly.
    void request_load_settings();

    /// Loads all registered textures.
    void load_texture_assets(std::string const& texture_asset_dir,
                             std::vector<Texture_asset_create_info>&& texture_assets);
    void load_material_palettes(
        std::vector<Material_asset_create_info>&& material_assets,
        std::vector<Material_palette_asset_create_info>&& material_palette_assets,
        Material_organizer& material_organizer);
    void load_model_assets(std::string const& afa_asset_dir,
                           std::vector<Model_asset_create_info>&& model_assets,
                           Render_model_data_collection& render_model_data_collection,
                           Material_organizer& material_organizer);

    /// Loads in deformed models using a combined model.
    void build_deformed_combined_model(Render_model_data_collection& render_model_data_collection);

    /// Creates joint transforms buffers for new deformed models.
    void create_joint_transforms_buffers(
        Render_model_data_collection& render_model_data_collection);

    /// Polls for input events.
    void poll_input_events();

    /// Builds one imgui frame using a callback and readies the frame for rendering.
    void build_imgui_contents(Camera_internal& camera,
                              std::vector<Render_view_size>& out_rend_view_sizes);

    /// Checks whether the render view sizes has changed from currently applied ones.
    bool check_render_view_sizes_changed(
        std::vector<Render_view_size> const& rend_view_sizes) const;

    /// Sets render view sizes for hdr draw images.
    void set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes);

    /// Sets GPU camera properties for a render view.
    void set_render_view_camera(size_t render_view_idx, mat4 camera_projection, mat4 camera_view);

    /// Sets GPU directional light properties for a render view.
    void set_directional_light(size_t render_view_idx,
                               vec3 direction,
                               vec3 color,
                               float_t intensity);

    /// Gets render view data for a render view.
    void* get_render_view(size_t rend_view_idx);

    /// Begins rendering while clearing the render view images.
    void begin_rendering_render_view(size_t rend_view_idx);

    /// Ends rendering a render view.
    void end_rendering_render_view(size_t rend_view_idx);

    /// Sets GPU per-instance data from list of render objects.
    void set_render_object_per_instance_data(
        Material_organizer const& material_organizer,
        std::vector<Render_object> const& rend_obj_list,
        std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
        size_t mod_mesh_ref_list_length);

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

    /// Target for where to render to for LDR rendering.
    enum Ldr_target
    {
        LDR_TARGET_SWAPCHAIN = 0,
        LDR_TARGET_IMGUI,
    };

    /// .
    void render_hdr_to_ldr_postprocessing(size_t rend_view_idx, Ldr_target render_target);

    /// Renders collected immediate-mode GUI commands to LDR present surface.
    void render_imgui();

    /// Waits until GPU can use next frame resources, then starts a new frame.
    bool start_next_frame();

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
