#pragma once

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
/// @note all of the methods within this class are thread-safe.
class Renderer
{
public:
    Renderer(std::string const& title, int32_t width, int32_t height);
    ~Renderer();  // For pimpl.

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Asset loading.

    /// Adds geometry material to renderer.
    void add_material(std::string const& material_name,
                      std::string const& shader_name,
                      std::unordered_map<std::string, std::string> const& shader_params);

    /// Adds set of geometry material to renderer index.
    void add_material_set(std::string const& mat_set_name, std::vector<std::string>&& materials);

    /// Adds model to renderer.
    void add_model(std::string const& model_name,
                   std::string const& file_ext,
                   std::string const& default_mat_set_name);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Render loop.

    /// Runs render loop until `shutdown_loop()` is called.
    /// @warning this must be called from the main thread.
    void run();

    /// Signals for renderer to shut down.
    void shutdown_loop();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Render object lifetime.

    /// Creates render object.
    pool_key_t create_render_obj(Render_obj_create_config&& config);

    /// Destroys render object.
    void destroy_render_obj(pool_key_t key);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Animator controls.

    // @TODO.

private:
    std::string m_title;
    int32_t m_width;
    int32_t m_height;

    std::atomic_bool m_shutdown_flag{ false };
};

}  // namespace TXP
