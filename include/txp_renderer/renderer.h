#pragma once

#include "btglm.h"
#include "entt/entity/fwd.hpp"
#include "txp_renderer/camera/camera.h"
#include "txp_renderer/types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

class Skeletal_animator;  // Forward decl.

/// Engine that handles render processes and presenting.
/// @warning this must run on the main thread due to windowing limitations.
/// @note "Asset loading" must be run prior to the `.run()` function.
/// @note "Render object lifetime" and "Animator controls" functions are thread-safe.
class Renderer
{
public:
    Renderer(entt::registry& ecs_registry,
             std::string const& title,
             int32_t width,
             int32_t height,
             std::string const& texture_asset_dir,
             std::string const& shader_asset_dir,
             std::string const& model_asset_dir,
             std::string const& afa_asset_dir,
             std::string const& animator_asset_dir,
             std::function<void(bool)>&& set_play_flag_fn,
             std::function<bool()>&& get_play_flag_fn);

    ~Renderer();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Render loop.

    /// Compiles assets and builds renderer.
    /// @NOTE: must be run prior to the other render loop functions.
    void build();

    /// Signals for renderer to shut down.
    void shutdown_loop();

    /// Check for whether request to shut down has been signaled.
    bool is_requesting_shutdown() const;

    /// Updates input state.
    void poll_input_events();

    /// Renders one singular frame then returns.
    void render_one_frame(float_t delta_time);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Asset loading.

    /// Adds texture to renderer.
    /// @note only ".ktx2" file extension is supported.
    void add_texture(std::string const& texture_name,
                     std::string const& file_ext);

    /// Adds geometry material to renderer.
    void add_material(std::string const& material_name,
                      std::string const& shader_name,
                      std::unordered_map<std::string, std::string> const& shader_params);

    /// Adds set of geometry material to renderer index.
    void add_material_palette(std::string const& mat_set_name,
                              std::vector<std::string>&& materials);

    /// Adds model to renderer.
    void add_model(std::string const& model_name,
                   std::string const& file_ext,
                   bool load_animator_template,
                   bool load_anim_frame_action);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Animator controls.

    /// Advances global AFA simulation timer (should be called before updating AFAs).
    static void advance_afa_sim_timer(float_t delta_time);

    /// Tries to get a skeletal animator from the ECS entity ID.
    std::optional<Skeletal_animator> try_get_skeletal_animator(entt::entity ecs_entity);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Model information.

    /// Model with only essential data.
    struct Basic_model
    {
        struct Basic_vertex
        {
            vec3 position;
        };
        std::vector<Basic_vertex> vertices;
        std::vector<uint32_t> indices;
    };

    /// Gets model's basic data.
    Basic_model get_model_basic_data(std::string const& model_name) const;

    /// Gets camera view direction.
    Camera& get_main_camera();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Debug special functions.

    /// Set the callback function for imgui build contents.
    void set_imgui_build_contents_callback(std::function<void()>&& callback);

    /// Can force a disable. (e.g. during level editor)
    void set_allow_deformed_render_models(bool allow);

    /// Reports the performance time for display in the debug stats.
    void report_performance_time(Performance_time_type perf_time_type, float_t delta_time);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
