#pragma once

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

/// Builds ImGui content for the frame.
void build_content(TXP::Input::Input_handler const& input_handler,
                   std::vector<Render_view_size>& out_rend_view_sizes);

}  // namespace editor_content
}  // namespace TXP
