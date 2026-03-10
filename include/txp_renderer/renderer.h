#pragma once

#include "camera/camera.h"
#include "entt/entity/fwd.hpp"
#include "mutex_wrapper/mutex_wrapper.h"
#include "render_object/render_object.h"
#include "txp_renderer/types.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

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
             std::string const& model_asset_dir);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Render loop.

    /// Runs render loop until `shutdown_loop()` is called.
    /// @warning this must be called from the main thread.
    void run();

    /// Signals for renderer to shut down.
    void shutdown_loop();

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
    void add_material_set(std::string const& mat_set_name, std::vector<std::string>&& materials);

    /// Adds model to renderer.
    void add_model(std::string const& model_name,
                   std::string const& file_ext);

#define POSSIBLY_REMOVE_THIS_LETS_SEE 0
#if POSSIBLY_REMOVE_THIS_LETS_SEE
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Render object lifetime.

    /// Creates render object.
    pool_key_t create_render_obj(Render_object_config&& config);

    /// Destroys render object.
    void destroy_render_obj(pool_key_t key);
#endif // POSSIBLY_REMOVE_THIS_LETS_SEE

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Animator controls.

    // @TODO.

private:
    entt::registry& m_ecs_registry;

    std::string m_title;
    int32_t m_width;
    int32_t m_height;

    std::string m_texture_asset_dir;
    std::string m_shader_asset_dir;
    std::string m_model_asset_dir;

    /// Able to register assets until assets are starting to be loaded into the GPU.
    std::atomic_bool m_asset_reg_window_open{ true };

    BT::Mutex_wrapper<std::vector<Texture_asset_create_info>> m_texture_assets;
    BT::Mutex_wrapper<std::vector<Material_asset_create_info>> m_material_assets;
    BT::Mutex_wrapper<std::vector<Material_set_asset_create_info>> m_material_set_assets;
    BT::Mutex_wrapper<std::vector<Model_asset_create_info>> m_model_assets;

    /// Loaded information of model assets.
    Render_model_data_collection m_render_model_data_collection;

    /// Flag for renderer to start shutdown process.
    std::atomic_bool m_shutdown_flag{ false };

    /// Camera for renderer and any other threads that desire to access it.
    Camera m_camera;
};

}  // namespace TXP
