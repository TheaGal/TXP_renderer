#pragma once

#if TXP_GFX_BACKEND_VULKAN
#include "vulkan/vulkan_core.h"
#endif // TXP_GFX_BACKEND_VULKAN

#include "renderer/types.h"

#include <vector>


namespace TXP
{

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

/// Builds ImGui content for the frame.
void build_content(TXP::Input::Input_handler const& input_handler,
                   Render_view_image_content const& render_view_image_content,
                   std::vector<Render_view_size>& out_rend_view_sizes);

}  // namespace editor_content
}  // namespace TXP
