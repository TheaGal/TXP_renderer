#pragma once

#include "btjson.h"


namespace TXP
{
namespace component
{

/// (DEV COMPONENT!!!) Editor agent for editing animation frame actions.
struct _Dev_animation_frame_action_editor_agent
{
    uint32_t working_anim_state_idx{ (uint32_t)-1 };

    // dummy
    int32_t dummy = 69;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        _Dev_animation_frame_action_editor_agent,
        dummy
    );
};

}  // namespace component
}  // namespace TXP
