#pragma once

#include <string>


namespace TXP
{
namespace GFX
{

/// Sets up the renderer platform-dependent backend and the render graph.
void setup_renderer(std::string const& title, int32_t width, int32_t height);

/// Tears down renderer, cleaning up resources.
void teardown_renderer();

/// Acquires next render image. Will block until an image becomes available.
uint32_t acquire_next_image();

}  // namespace GFX
}  // namespace TXP
