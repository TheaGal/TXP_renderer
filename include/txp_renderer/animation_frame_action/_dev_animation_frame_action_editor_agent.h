#pragma once

#include "btjson.h"


namespace TXP
{
namespace component
{

/// (DEV COMPONENT!!!) Editor agent for editing animation frame actions.
struct _Dev_animation_frame_action_editor_agent
{
    bool request_reset_editor_state{ true };

    // Editor state.
    std::string prev_working_model_name;
    uint32_t working_anim_state_idx{ (uint32_t)-1 };
    size_t prev_anim_frame{ (size_t)-1 };
    void const* prev_working_afa_ctrls_copy{ nullptr };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        _Dev_animation_frame_action_editor_agent,
        request_reset_editor_state
    );
};

}  // namespace component
}  // namespace TXP
