#pragma once

#include "entt/entity/fwd.hpp"


namespace TXP
{
namespace system
{

/// Updates the agent and editor state for the animation frame action editor.
/// @NOTE: Only runs when imgui view is set to the anim frame action editor.
void _dev_animation_frame_action_editor(entt::registry& reg);

}  // namespace system
}  // namespace TXP
