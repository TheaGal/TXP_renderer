#pragma once


namespace TXP
{

namespace Input
{
class Input_handler;  // Forward decl.
}  // namespace Input


namespace editor_content
{

/// Builds ImGui content for the frame.
void build_content(TXP::Input::Input_handler const& input_handler);

}  // namespace editor_content
}  // namespace TXP
