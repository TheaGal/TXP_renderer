#include "txp_renderer/renderer.h"

#include "btservice_finder.h"
#include "bttimer.h"
#include "camera/camera.h"
#include "entt/entity/registry.hpp"
#include "gfx.h"
#include "material_organizer/material_organizer.h"
#include "mutex_wrapper/mutex_wrapper.h"
#include "render_object/render_model.h"
#include "render_object/render_object.h"
#include "renderer/types.h"
#include "shader/shader_basic_diffuse.h"
#include "shader/shader_gradient.h"
#include "txp_renderer/types.h"

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <unordered_map>


namespace TXP
{

// struct Renderer::Impl
struct Renderer::Impl
{
    Impl(entt::registry& ecs_registry,
         std::string const& title,
         int32_t width,
         int32_t height,
         std::string const& texture_asset_dir,
         std::string const& shader_asset_dir,
         std::string const& model_asset_dir)
        : ecs_registry(ecs_registry)
        , title(title)
        , width(width)
        , height(height)
        , texture_asset_dir(texture_asset_dir)
        , shader_asset_dir(shader_asset_dir)
        , model_asset_dir(model_asset_dir)
    {
    }

    entt::registry& ecs_registry;

    std::string title;
    int32_t width;
    int32_t height;

    std::string texture_asset_dir;
    std::string shader_asset_dir;
    std::string model_asset_dir;

    /// Able to register assets until assets are starting to be loaded into the GPU.
    std::atomic_bool asset_reg_window_open{ true };

    BT::Mutex_wrapper<std::vector<Texture_asset_create_info>> texture_assets;
    BT::Mutex_wrapper<std::vector<Material_asset_create_info>> material_assets;
    BT::Mutex_wrapper<std::vector<Material_palette_asset_create_info>> material_palette_assets;
    BT::Mutex_wrapper<std::vector<Model_asset_create_info>> model_assets;

    /// Material information tracker.
    Material_organizer material_organizer;

    /// Loaded information of model assets.
    Render_model_data_collection render_model_data_collection;

    /// Flag for renderer to start shutdown process.
    std::atomic_bool shutdown_flag{ false };

    /// Camera for renderer and any other threads that desire to access it.
    Camera camera;
};


// class Renderer
Renderer::Renderer(entt::registry& ecs_registry,
                   std::string const& title,
                   int32_t width,
                   int32_t height,
                   std::string const& texture_asset_dir,
                   std::string const& shader_asset_dir,
                   std::string const& model_asset_dir)
    : m_pimpl(std::make_unique<Impl>(ecs_registry,
                                     title,
                                     width,
                                     height,
                                     texture_asset_dir,
                                     shader_asset_dir,
                                     model_asset_dir))
{   // Ensure only one instance.
    static std::atomic_bool s_init{ false };
    bool expect_init{ false };
    if (!s_init.compare_exchange_strong(expect_init, true))
    {
        throw std::runtime_error("Only one `TXP::Renderer` may be created.");
    }

    // Small setup of auxiliary systems.
    Shader_Creation::set_shader_directory(m_pimpl->shader_asset_dir);
    set_model_directory(m_pimpl->model_asset_dir);

    // Add self as service.
    BT_SERVICE_FINDER_ADD_SERVICE(Renderer, this);
}

Renderer::~Renderer() = default;  // for pimpl.


////////////////////////////////////////////////////////////////////////////////////////////////////
// Render loop.
void Renderer::run()
{
    auto& m{ *m_pimpl };

    // Setup renderer.
    Graphics g(m.title, m.width, m.height);

    m.asset_reg_window_open = false;

    // Load textures.
    g.load_texture_assets(m.texture_asset_dir, std::move(*m.texture_assets.scoped_lock()));

    // Create shaders.
    // @TODO: @THINK: perhaps these shaders could be under an abstract class if there's a similar
    //                enough of an interface.
    Shader::Shader_gradient shad_gradient{ m.material_organizer, g.get_impl() };
    Shader::Shader_basic_diffuse shad_basic_diffuse{ m.material_organizer,
                                                     m.render_model_data_collection,
                                                     g.get_impl() };

    // Insert material params.
    auto material_assets{ m.material_assets.scoped_lock() };
    for (auto const& mat_asset : *material_assets)
    {   // Find shader.
        // clang-format off
        if      (mat_asset.shader_name == shad_gradient.k_name)       shad_gradient.make_material(mat_asset.material_name, mat_asset.shader_params);
        else if (mat_asset.shader_name == shad_basic_diffuse.k_name)  shad_basic_diffuse.make_material(mat_asset.material_name, mat_asset.shader_params);
        else throw std::runtime_error("Unknown shader name");
        // clang-format on
    }

    // Organize material param collections into shaders.
    shad_gradient.organize_materials();
    shad_basic_diffuse.organize_materials();

    // Load material palettes (aligns with meshes inside models to assign materials).
    g.load_material_palettes(std::move(*material_assets),
                             std::move(*m.material_palette_assets.scoped_lock()),
                             m.material_organizer);

    // Load models.
    g.load_model_assets(std::move(*m.model_assets.scoped_lock()),
                        m.render_model_data_collection,
                        m.material_organizer);

    // Timer.
    BT::Timer main_timer;
    main_timer.start_timer();

    // List of render objects.
    std::vector<Render_object> render_object_list;

    std::vector<Render_object_model_mesh_reference> model_mesh_ref_list;
    model_mesh_ref_list.resize(65535);  // @NOTE: from per-instance data max entries.

    // Render frames until shutdown flag is tripped.
    while (!m.shutdown_flag.load())
    {
        BT::logger::notify_start_new_mainloop_iteration();
        float_t delta_time{ main_timer.calc_delta_time() };

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Process render object changes.
        // @TODO: put this into its own func.
        // @TODO: process this once per tick (e.g. 1/60th second) (i.e. at the same rate as the
        //        simulation loop)

        bool appended_render_object{ false };
        bool removed_render_object{ false };

        // Mark all as stale.
        for (auto& rend_obj : render_object_list)
        {
            rend_obj.is_stale = true;
        }

        // Add new render objects and mark existing render objects as non-stale.
        auto rend_obj_cfg_view = m.ecs_registry.view<TXP::Render_object_config>();
        for (auto ecs_entity : rend_obj_cfg_view)
        {
            auto& rend_obj_cfg = rend_obj_cfg_view.get<TXP::Render_object_config>(ecs_entity);
            auto& rend_owned_data = rend_obj_cfg.renderer_owned_data;

            if (rend_owned_data.pool_key == k_pool_key_process_flag)
            {   // Need to create new render object.
                rend_obj_cfg.renderer_owned_data.pool_key = render_object_list.size();
                render_object_list.emplace_back(Render_object{
                    .layer = rend_obj_cfg.layer,
                    .render_model_idx =
                        m.render_model_data_collection.get_static_model_data_set_idx(
                            rend_obj_cfg.model_name),
                    .material_palette_idx = m.material_organizer.get_material_palette_idx(
                        !rend_obj_cfg.material_palette.empty()
                            ? rend_obj_cfg.material_palette
                            : rend_obj_cfg.model_name + "__default_material_palette_name__"),
                });
            }
            else
            {   // Mark as non-stale.
                render_object_list[rend_obj_cfg.renderer_owned_data.pool_key].is_stale = false;
            }

            // Update transform.
            // @TODO: adding interpolation here (i.e. just copying both the a->b transforms).
            glm_mat4_copy(rend_obj_cfg.transform.raw,
                          render_object_list[rend_obj_cfg.renderer_owned_data.pool_key].transform);
        }

        // Delete stale render objects.
        std::unordered_map<int32_t, int32_t> old_to_new_idx_map;
        old_to_new_idx_map.reserve(rend_obj_cfg_view.size());

        for (auto i = static_cast<int32_t>(render_object_list.size()) - 1; i >= 0; i--)
        {
            if (render_object_list[i].is_stale)
            {
                render_object_list.erase(render_object_list.begin() + i);
                removed_render_object = true;
            }
            else
            {
                old_to_new_idx_map.emplace(i, old_to_new_idx_map.size());
            }
        }
        for (auto& [_, new_idx] : old_to_new_idx_map)
        {   // Flip since last loop was iterating backwards.
            new_idx = (old_to_new_idx_map.size() - 1 - new_idx);
        }

        // Apply new pool keys.
        for (auto ecs_entity : rend_obj_cfg_view)
        {
            auto& rend_obj_cfg = rend_obj_cfg_view.get<TXP::Render_object_config>(ecs_entity);

            rend_obj_cfg.renderer_owned_data.pool_key =
                old_to_new_idx_map.at(rend_obj_cfg.renderer_owned_data.pool_key);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Update renderer-timed things.

        // Update animator timers (renderer-profile).
        // @TODO

        // Calculate animator joints.
        // @TODO

        // Poll for input events.
        g.poll_input_events();
        m.camera.update(delta_time);

        // Build imgui for this frame.
        std::vector<Render_view_size> render_view_sizes;
        g.build_imgui_contents(m.camera, render_view_sizes);

        bool render_view_sizes_changed = g.check_render_view_sizes_changed(render_view_sizes);

        // Wait until can start rendering.
        bool frame_acquired{ false };
        if (!render_view_sizes_changed)
            frame_acquired = g.start_next_frame();

        // Set render view sizes.
        g.set_render_view_sizes(render_view_sizes);
        m.camera.set_render_view_sizes(render_view_sizes);

        // Avoid drawing with deleted GPU data or frame acquire failed.
        if (render_view_sizes_changed || !frame_acquired)
            continue;

        // Populate render object model mesh references for organizing per-instance data.
        size_t cur_modmesh_ref_idx{ 0 };
        shad_gradient.allocate_per_instance_data_slots(render_object_list,
                                                       model_mesh_ref_list,
                                                       cur_modmesh_ref_idx);
        shad_basic_diffuse.allocate_per_instance_data_slots(render_object_list,
                                                            model_mesh_ref_list,
                                                            cur_modmesh_ref_idx);

        // Set per-instance data from render objects.
        g.set_render_object_per_instance_data(m.material_organizer,
                                              render_object_list,
                                              model_mesh_ref_list,
                                              cur_modmesh_ref_idx);

        // Render for each render view.
        size_t render_view_idx{ 0 };
        for (auto const& cam_matrix : m.camera.calc_cam_matrices())
        {
            bool main_cam_matrix{ render_view_idx == 0 };

            auto render_view{ g.get_render_view(render_view_idx) };

            g.set_render_view_camera(render_view_idx,
                                     const_cast<vec4*>(cam_matrix.projection),
                                     const_cast<vec4*>(cam_matrix.view));
            g.set_directional_light(render_view_idx,
                                    vec3{ 0.742781, 0.557086, 0.371391 },  // @HARDCODE
                                    vec3{ 255.0f / 255.0f, 228.0f / 255.0f, 206.0f / 255.0f },
                                    10.0f);

            if (main_cam_matrix)
            {
                // g.compute_light_culling();
                // g.compute_shadow_culling();
                // g.compute_opaque_geometry_culling();
                // g.compute_transparent_geometry_culling();
            }

            // g.render_shadows(render_view);
            // if (main_cam_matrix)  // @TEMP: @TODO: when render view resizes, the descriptor set for this compute shader needs to get recreated.
            //     shad_gradient.compute(render_view);  // @TODO: this needs to get changed to image-type GENERAL before compute shader usage.

            // Render all graphics shaders.
            g.begin_rendering_render_view(render_view_idx);
            shad_basic_diffuse.draw(render_object_list, model_mesh_ref_list, render_view);
            g.end_rendering_render_view(render_view_idx);

            if (main_cam_matrix)
            {
                // g.render_clouds();
                // g.render_volumetric_light();
            }

            // g.render_particles();
            // g.render_transparent_geometry();

            g.render_hdr_to_ldr_postprocessing(render_view_idx, g.LDR_TARGET_IMGUI);

            render_view_idx++;
        }
        if (render_view_idx == 0)
        {
            throw std::runtime_error("Main render view must exist.");
        }

        g.render_imgui();

        g.present_frame_to_screen();
    }

    // Wait until GPU is idle before destruction.
    g.wait_until_gpu_idle();
}

void Renderer::shutdown_loop()
{
    m_pimpl->shutdown_flag.store(true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Asset loading.
void Renderer::add_texture(std::string const& texture_name, std::string const& file_ext)
{
    if (!m_pimpl->asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    if (file_ext != ".ktx2")
        throw std::runtime_error("Only .ktx2 file type for textures are allowed.");

    m_pimpl->texture_assets.scoped_lock()->emplace_back(texture_name, texture_name + ".ktx2");
}

void Renderer::add_material(std::string const& material_name,
                            std::string const& shader_name,
                            std::unordered_map<std::string, std::string> const& shader_params)
{
    if (!m_pimpl->asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_pimpl->material_assets.scoped_lock()->emplace_back(material_name, shader_name, shader_params);
}

void Renderer::add_material_palette(std::string const& mat_set_name,
                                std::vector<std::string>&& materials)
{
    if (!m_pimpl->asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_pimpl->material_palette_assets.scoped_lock()->emplace_back(mat_set_name, std::move(materials));
}

void Renderer::add_model(std::string const& model_name,
                         std::string const& file_ext)
{
    if (!m_pimpl->asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_pimpl->model_assets.scoped_lock()->emplace_back(model_name, file_ext);
}

}  // namespace TXP
