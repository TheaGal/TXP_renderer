#include "txp_renderer/renderer.h"

#include "animation_frame_action/runtime_data_controls.h"
#include "btdatecheck.h"
#include "btservice_finder.h"
#include "camera/camera_internal.h"
#include "entt/entity/registry.hpp"
#include "gfx.h"
#include "material_organizer/material_organizer.h"
#include "mutex_wrapper/mutex_wrapper.h"
#include "render_object/animator_template.h"
#include "render_object/render_model.h"
#include "render_object/render_object.h"
#include "render_object/skeletal_animator.h"
#include "renderer/types.h"
#include "shader/shader_basic_diffuse.h"
#include "shader/shader_gradient.h"
#include "shader/shader_skinned_model.h"
#include "shader_creation/shader_creation.h"
#include "txp_renderer/animator/skeletal_animator.h"
#include "txp_renderer/types.h"

#include <atomic>
#include <cassert>
#include <memory>
#include <optional>
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
         std::string const& model_asset_dir,
         std::string const& afa_asset_dir,
         std::string const& animator_asset_dir,
         std::function<void(bool)>&& set_play_flag_fn,
         std::function<bool()>&& get_play_flag_fn)
        : ecs_registry(ecs_registry)
        , anim_template_bank(animator_asset_dir)
        , title(title)
        , width(width)
        , height(height)
        , texture_asset_dir(texture_asset_dir)
        , shader_asset_dir(shader_asset_dir)
        , model_asset_dir(model_asset_dir)
        , afa_asset_dir(afa_asset_dir)
        , set_play_flag_fn(std::move(set_play_flag_fn))
        , get_play_flag_fn(std::move(get_play_flag_fn))
    {
    }

    ~Impl()
    {
        // Wait until GPU is idle before destruction.
        graphics->wait_until_gpu_idle();
    }

    entt::registry& ecs_registry;

    Animator_template_bank anim_template_bank;

    Hitcapsule_group_overlap_solver hitcapsule_solver;  // Just needs to be created and stored somewhere.  -Thea 2026/04/05

    std::string title;
    int32_t width;
    int32_t height;

    std::string texture_asset_dir;
    std::string shader_asset_dir;
    std::string model_asset_dir;
    std::string afa_asset_dir;

    std::function<void(bool)> set_play_flag_fn;
    std::function<bool()> get_play_flag_fn;

    /// Able to register assets until assets are starting to be loaded into the GPU.
    std::atomic_bool asset_reg_window_open{ true };

    BT::Mutex_wrapper<std::vector<Texture_asset_create_info>> texture_assets;
    BT::Mutex_wrapper<std::vector<Material_asset_create_info>> material_assets;
    BT::Mutex_wrapper<std::vector<Material_palette_asset_create_info>> material_palette_assets;
    BT::Mutex_wrapper<std::vector<Model_asset_create_info>> model_assets;

    /// Built renderer.
    std::unique_ptr<Graphics> graphics;

    /// Material information tracker.
    Material_organizer material_organizer;

    /// Loaded information of model assets.
    Render_model_data_collection render_model_data_collection;

    /// Shaders.
    std::unique_ptr<Shader::Shader_gradient> shad_gradient;
    std::unique_ptr<Shader::Shader_skinned_model> shad_skinned_model;
    std::unique_ptr<Shader::Shader_basic_diffuse> shad_basic_diffuse;

    /// List of render objects.
    std::vector<Render_object> render_object_list;

    /// Reference list of model meshes.
    std::vector<Render_object_model_mesh_reference> model_mesh_ref_list;

    /// Flag for renderer to start shutdown process.
    std::atomic_bool shutdown_flag{ false };

    /// Camera for renderer and any other threads that desire to access it.
    Camera_internal camera;

    /// Public-facing camera interface.
    Camera public_external_camera;

    /// Flag for disabling deformed render models.
    std::atomic_bool allow_deformed_render_models{ true };

    /// Internal current config of allowing deformed render models.
    bool internal_allowing_deformed_render_models{ allow_deformed_render_models };

    /// Debug stats' performance times.
    std::unordered_map<Performance_time_type, float_t> perf_time_map;


    // ---- Functions ------------------------------------------------------------------------------

    void process_render_object_changes()
    {
        // @TODO: process this once per tick (e.g. 1/60th second) (i.e. at the same rate as the
        //        simulation loop)

        auto& m{ *this };  // @HACK
        auto& g{ *m.graphics };

        bool appended_render_object{ false };
        bool removed_render_object{ false };
        bool deformed_model_count_changed{ false };

        // Mark all as stale.
        for (auto& rend_obj : m.render_object_list)
        {
            rend_obj.is_stale = true;
        }

        // Get all render obj configs.
        auto rend_obj_cfg_view = m.ecs_registry.view<TXP::component::Render_object_config>();

        // Check for a config switch.
        if (m.internal_allowing_deformed_render_models != m.allow_deformed_render_models.load())
        {
            // Switch configuration; let all rend objs go stale for this tick.
            m.internal_allowing_deformed_render_models =
                !m.internal_allowing_deformed_render_models;
        }
        else
        {   // Add new render objects and mark existing render objects as non-stale.
            for (auto ecs_entity : rend_obj_cfg_view)
            {
                auto& rend_obj_cfg =
                    rend_obj_cfg_view.get<component::Render_object_config>(ecs_entity);
                auto& rend_owned_data = rend_obj_cfg.renderer_owned_data;

                if (rend_owned_data.pool_key == k_pool_key_process_flag)
                {   // Need to create new render object.
                    rend_owned_data.pool_key = m.render_object_list.size();

                    uint16_t new_render_model_idx{ (uint16_t)-1 };
                    if (m.internal_allowing_deformed_render_models && rend_obj_cfg.is_deformed)
                    {   // Create new deformed model.
                        auto static_model_data_set_idx =
                            m.render_model_data_collection.get_static_model_data_set_idx(
                                rend_obj_cfg.model_name);

                        new_render_model_idx =
                            m.render_model_data_collection
                                .create_deformed_model_from_static_model_data_set(
                                    rend_obj_cfg.model_name);

                        // Add animator.
                        bool has_root_motion_tag{
                            m.ecs_registry.any_of<component::Animator_root_motion>(ecs_entity)
                        };

                        auto& skeletal_animator{
                            m.ecs_registry.emplace<component_internal::Model_animator>(
                                ecs_entity,
                                m.render_model_data_collection.get_deformed_model_anim_set(
                                    m.render_model_data_collection.get_deformed_model_anim_set_idx(
                                        rend_obj_cfg.model_name)),
                                m.render_model_data_collection.get_deformed_model_data_set(
                                    new_render_model_idx),
                                has_root_motion_tag)
                        };

                        BT::service_finder::find_service<Animator_template_bank>()
                            .load_animator_template_into_animator(skeletal_animator,
                                                                  rend_obj_cfg.model_name);

                        // Set first anim as default state-set.
                        assert(!skeletal_animator.get_animator_states().empty());
                        skeletal_animator.change_state_set(
                            { .anim_state_indices = { 0 }, .loop_final_state = true });

                        // Add anim frame action controller.
                        if (anim_frame_action::Bank::has(rend_obj_cfg.model_name))
                        {
                            auto& afa_ctrller_ref{ anim_frame_action::Bank::get(
                                rend_obj_cfg.model_name) };

                            skeletal_animator.configure_anim_frame_action_controls(
                                &afa_ctrller_ref,
                                m.ecs_registry.get<component::Entity_metadata>(ecs_entity).uuid,
                                component_internal::Model_animator::
                                    make_jump_queue_create_list_from_anim_frame_action_controls(
                                        afa_ctrller_ref));

                            // Add hitcapsule set driver.
                            m.ecs_registry
                                .emplace_or_replace<component::Animator_driven_hitcapsule_set>(
                                    ecs_entity);
                        }

                        deformed_model_count_changed = true;
                    }
                    else
                    {
                        new_render_model_idx =
                            m.render_model_data_collection.get_static_model_data_set_idx(
                                rend_obj_cfg.model_name);
                    }
                    m.render_model_data_collection.report_one_user_added(new_render_model_idx);

                    m.render_object_list.emplace_back(Render_object{
                        .layer = rend_obj_cfg.render_layer,
                        .render_model_idx = new_render_model_idx,
                        .material_palette_idx = m.material_organizer.get_material_palette_idx(
                            !rend_obj_cfg.material_palette.empty()
                                ? rend_obj_cfg.material_palette
                                : rend_obj_cfg.model_name + "__default_material_palette_name__"),
                    });
                    appended_render_object = true;
                }
                else
                {   // Mark as non-stale.
                    m.render_object_list[rend_owned_data.pool_key].is_stale = false;
                }

                // Update transform.
                // @TODO: adding interpolation here (i.e. just copying both the a->b transforms).
                glm_mat4_copy(rend_obj_cfg.transform.raw,
                              m.render_object_list[rend_owned_data.pool_key].transform);
            }
        }

        // Delete stale render objects.
        std::unordered_map<int32_t, int32_t> old_to_new_idx_map;
        old_to_new_idx_map.reserve(rend_obj_cfg_view.size());

        for (auto i = static_cast<int32_t>(m.render_object_list.size()) - 1; i >= 0; i--)
        {
            if (m.render_object_list[i].is_stale)
            {
                bool is_model_unused;
                m.render_model_data_collection.report_one_user_removed(
                    m.render_object_list[i].render_model_idx,
                    is_model_unused);

                if (is_model_unused && m.render_object_list[i].is_animated())
                {
                    deformed_model_count_changed = true;
                    assert(false);  // @TODO: ensure that the model gets deleted as well. maybe make a `prune()` func in the render model data collection?
                }

                m.render_object_list.erase(m.render_object_list.begin() + i);
                removed_render_object = true;

                // In case rend obj does exist still but just is going to be recreated.
                old_to_new_idx_map.emplace(i, k_pool_key_process_flag);
            }
            else
            {
                old_to_new_idx_map.emplace(i, old_to_new_idx_map.size());
            }
        }
        for (auto& elem : old_to_new_idx_map)
        {   // Flip since last loop was iterating backwards.
            auto& new_idx{ elem.second };
            if (new_idx != k_pool_key_process_flag)
                new_idx = (old_to_new_idx_map.size() - 1 - new_idx);
        }

        // Apply new pool keys.
        for (auto ecs_entity : rend_obj_cfg_view)
        {
            auto& rend_obj_cfg =
                rend_obj_cfg_view.get<component::Render_object_config>(ecs_entity);

            rend_obj_cfg.renderer_owned_data.pool_key =
                old_to_new_idx_map.at(rend_obj_cfg.renderer_owned_data.pool_key);
            
            if (rend_obj_cfg.renderer_owned_data.pool_key == k_pool_key_process_flag)
            {   // Remove skeletal animator if still attached to render object.
                m.ecs_registry.remove<component_internal::Model_animator>(ecs_entity);
            }
        }

        // Ensure that deformed models are created/removed.
        if (deformed_model_count_changed)
        {
            g.wait_until_gpu_idle();
            g.build_deformed_combined_model(m.render_model_data_collection);
            g.create_joint_transforms_buffers(m.render_model_data_collection);
            m.shad_skinned_model->build_combined_deformed_vertex_set_descriptor_set();
        }
    }
};


// class Renderer
Renderer::Renderer(entt::registry& ecs_registry,
                   std::string const& title,
                   int32_t width,
                   int32_t height,
                   std::string const& texture_asset_dir,
                   std::string const& shader_asset_dir,
                   std::string const& model_asset_dir,
                   std::string const& afa_asset_dir,
                   std::string const& animator_asset_dir,
                   std::function<void(bool)>&& set_play_flag_fn,
                   std::function<bool()>&& get_play_flag_fn)
    : m_pimpl(std::make_unique<Impl>(ecs_registry,
                                     title,
                                     width,
                                     height,
                                     texture_asset_dir,
                                     shader_asset_dir,
                                     model_asset_dir,
                                     afa_asset_dir,
                                     animator_asset_dir,
                                     std::move(set_play_flag_fn),
                                     std::move(get_play_flag_fn)))
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
void Renderer::build()
{
    auto& m{ *m_pimpl };

    // Setup renderer.
    m.graphics = std::make_unique<Graphics>(
        m.title,
        m.width,
        m.height,
        Information_hook_struct{
            .set_play_flag_fn = m.set_play_flag_fn,
            .get_play_flag_fn = m.get_play_flag_fn,
            .perf_time_map = m.perf_time_map,
        });
    auto& g{ *m.graphics };

    m.asset_reg_window_open = false;

    // Load textures.
    g.load_texture_assets(m.texture_asset_dir, std::move(*m.texture_assets.scoped_lock()));

    // Create shaders.
    // @TODO: @THINK: perhaps these shaders could be under an abstract class if there's a similar
    //                enough of an interface.
    m.shad_gradient = std::make_unique<Shader::Shader_gradient>(m.material_organizer, g.get_impl());
    m.shad_skinned_model =
        std::make_unique<Shader::Shader_skinned_model>(m.material_organizer,
                                                       m.render_model_data_collection,
                                                       g.get_impl());
    m.shad_basic_diffuse =
        std::make_unique<Shader::Shader_basic_diffuse>(m.material_organizer,
                                                       m.render_model_data_collection,
                                                       g.get_impl());

    // Insert material params.
    auto material_assets{ m.material_assets.scoped_lock() };
    for (auto const& mat_asset : *material_assets)
    {   // Find shader.
        // clang-format off
        if      (mat_asset.shader_name == m.shad_gradient->k_name)       m.shad_gradient->make_material(mat_asset.material_name, mat_asset.shader_params);
        else if (mat_asset.shader_name == m.shad_basic_diffuse->k_name)  m.shad_basic_diffuse->make_material(mat_asset.material_name, mat_asset.shader_params);
        else throw std::runtime_error("Unknown shader name");
        // clang-format on
    }

    // Organize material param collections into shaders.
    m.shad_gradient->organize_materials();
    m.shad_basic_diffuse->organize_materials();

    // Load material palettes (aligns with meshes inside models to assign materials).
    g.load_material_palettes(std::move(*material_assets),
                             std::move(*m.material_palette_assets.scoped_lock()),
                             m.material_organizer);

    // Load models.
    g.load_model_assets(m.afa_asset_dir,
                        std::move(*m.model_assets.scoped_lock()),
                        m.render_model_data_collection,
                        m.material_organizer);

    // Init list of render objects.
    m.render_object_list.clear();
    m.model_mesh_ref_list.resize(65535);  // @NOTE: from per-instance data max entries.
}

void Renderer::shutdown_loop()
{
    m_pimpl->shutdown_flag.store(true);
}

bool Renderer::is_requesting_shutdown() const
{
    return m_pimpl->shutdown_flag.load();
}

void Renderer::poll_input_events()
{
    auto& m{ *m_pimpl };
    auto& g{ *m.graphics };

    g.poll_input_events();
}

void Renderer::render_one_frame(float_t delta_time)
{
    auto& m{ *m_pimpl };
    auto& g{ *m.graphics };

    BT::logger::notify_start_new_mainloop_iteration();

    m.process_render_object_changes();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Update renderer-timed things.

    // Update animator timers (renderer-profile).
    // @TODO

    // Calculate animator joints.
    // @TODO

    // Poll for input events.
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
        return;

    // Update animators.
    for (auto&& [_, animator] : m.ecs_registry.view<component_internal::Model_animator>().each())
    {   // Tick animator.
        animator.update(TXP::RENDERER_TIMER_PROFILE, delta_time);

        std::vector<mat4s> joint_matrices;
        animator.calc_anim_pose(TXP::RENDERER_TIMER_PROFILE,
                                animator.get_is_using_root_motion(),
                                joint_matrices);

        auto& def_mod{ animator.get_deformed_model() };

        // Update joint transform buffer.
        std::memcpy(def_mod.joint_transforms_buffer.get_p_mapped_data(),
                    joint_matrices.data(),
                    joint_matrices.size() * sizeof(mat4s));

        // Execute compute shader for corresponding deformed model,
        m.shad_skinned_model->compute(&def_mod);
    }

    // Populate render object model mesh references for organizing per-instance data.
    size_t cur_modmesh_ref_idx{ 0 };
    m.shad_gradient->allocate_per_instance_data_slots(m.render_object_list,
                                                      m.model_mesh_ref_list,
                                                      cur_modmesh_ref_idx);
    m.shad_basic_diffuse->allocate_per_instance_data_slots(m.render_object_list,
                                                           m.model_mesh_ref_list,
                                                           cur_modmesh_ref_idx);

    // Set per-instance data from render objects.
    g.set_render_object_per_instance_data(m.material_organizer,
                                          m.render_object_list,
                                          m.model_mesh_ref_list,
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
        //     m.shad_gradient->compute(render_view);  // @TODO: this needs to get changed to image-type GENERAL before compute shader usage.

        // Render all graphics shaders.
        g.begin_rendering_render_view(render_view_idx);
        m.shad_basic_diffuse->draw(m.render_object_list, m.model_mesh_ref_list, render_view);
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

    m_pimpl->material_palette_assets.scoped_lock()->emplace_back(mat_set_name,
                                                                 std::move(materials));
}

void Renderer::add_model(std::string const& model_name,
                         std::string const& file_ext,
                         bool load_animator_template,
                         bool load_anim_frame_action)
{
    if (!m_pimpl->asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_pimpl->model_assets.scoped_lock()->emplace_back(model_name,
                                                      file_ext,
                                                      load_animator_template,
                                                      load_anim_frame_action);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Animator controls.

void Renderer::advance_afa_sim_timer(float_t delta_time)
{
    component_internal::Model_animator::advance_sim_timer(delta_time);
}

std::optional<Skeletal_animator> Renderer::try_get_skeletal_animator(entt::entity ecs_entity)
{
    auto* animator = m_pimpl->ecs_registry.try_get<component_internal::Model_animator>(ecs_entity);

    if (!animator)
        return std::nullopt;  // Return failed optional.

    // Wrap internal animator.
    return Skeletal_animator{ animator };
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Model information.

auto Renderer::get_model_basic_data(std::string const& model_name) const -> Basic_model
{
    auto const& static_model_dat_set =
        m_pimpl->render_model_data_collection.get_static_model_data_set(
            m_pimpl->render_model_data_collection.get_static_model_data_set_idx(model_name));

    Basic_model basic_model;

    basic_model.vertices.reserve(static_model_dat_set.vertices.size());
    for (auto const& vert : static_model_dat_set.vertices)
    {
        basic_model.vertices.emplace_back();
        glm_vec3_copy(const_cast<TXP::Vertex&>(vert).position_vec3(),
                      basic_model.vertices.back().position);
    }

    for (auto const& mesh : static_model_dat_set.meshes)
    for (auto idx : mesh.indices)
    {
        basic_model.indices.emplace_back(idx);
    }
    basic_model.indices.shrink_to_fit();

    return basic_model;
}

Camera& Renderer::get_main_camera()
{
    return m_pimpl->public_external_camera;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug special functions.

void Renderer::set_allow_deformed_render_models(bool allow)
{
    m_pimpl->allow_deformed_render_models.store(allow);
}

void Renderer::report_performance_time(Performance_time_type perf_time_type, float_t delta_time)
{
    BT::date_deadline(2026, 8, 10);  // @TODO: do something with this time!!! Like display in a debug window??
    m_pimpl->perf_time_map[perf_time_type] = delta_time;
}

}  // namespace TXP
