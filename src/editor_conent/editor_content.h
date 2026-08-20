#pragma once

#if TXP_GFX_BACKEND_VULKAN
#include "vulkan/vulkan_core.h"
#endif // TXP_GFX_BACKEND_VULKAN

#include "renderer/types.h"

#include <functional>
#include <vector>


namespace TXP
{

struct Renderer_settings;  // Forward decl.
class Camera_internal;  // Forward decl.
struct Information_hook_struct;  // Forward decl.

namespace Input
{
class Input_handler;  // Forward decl.
}  // namespace Input


namespace editor_content
{

/// Content needed for render view image displaying.
struct Render_view_image_content
{
#if TXP_GFX_BACKEND_VULKAN
    std::vector<VkDescriptorSet> content_image_descriptors;
#endif // TXP_GFX_BACKEND_VULKAN
};

/// Set the callback function for imgui build contents.
void set_imgui_build_contents_callback(std::function<void()>&& callback);

/// Enables or disables ImGuizmo.
void set_imguizmo_enabled(bool flag);

/// Adds transform to manipulate with ImGuizmo next tick (note: if exec within the imgui
/// callback, it will get processed the same tick).
void add_to_imguizmo_manipulate(mat4 transform,
                                std::function<void(mat4 const)>&& changed_callback);

/// Builds ImGui content for the frame.
void build_content(TXP::Renderer_settings& settings,
                   TXP::Input::Input_handler const& input_handler,
                   Render_view_image_content const& render_view_image_content,
                   std::function<void(bool)> const& lock_cursor_fn,
                   Camera_internal& camera,
                   Information_hook_struct const& info_hook_struct,
                   std::vector<Render_view_size>& out_rend_view_sizes);

}  // namespace editor_content
}  // namespace TXP
