#pragma once

#include "txp_renderer/types.h"
#include <cstdint>
#include <memory>
#include <string>


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

    //
    // Render loop.
    //

    /// Runs render loop until `shutdown_loop()` is called.
    /// @warning this must be called from the main thread.
    void run();

    /// Signals for renderer to shut down.
    void shutdown_loop();

    //
    // Render object lifetime.
    //

    /// Creates render object.
    pool_key_t create_render_obj(Render_obj_create_config&& config);

    /// Destroys render object.
    void destroy_render_obj(pool_key_t key);

    //
    // Animator controls.
    //

    // @TODO.

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
