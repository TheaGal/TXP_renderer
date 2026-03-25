#pragma once

#include "entt/entity/fwd.hpp"

#include <cstdint>
#include <memory>
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

    ~Renderer();

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
    void add_material_palette(std::string const& mat_set_name,
                              std::vector<std::string>&& materials);

    /// Adds model to renderer.
    void add_model(std::string const& model_name,
                   std::string const& file_ext);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Animator controls.

    // @TODO.

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
