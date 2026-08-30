#pragma once

#include "btuuid.h"

#include <string>
#include <map>


namespace TXP
{

// class Model;  // ???

namespace component_internal
{
class Model_animator;
}  // namespace component_internal

namespace anim_frame_action
{

struct Runtime_data_controls;

struct Editor_state
{
    bool is_editor_state_untouched{ true };  // Set to true when editor state is reset.

    Runtime_data_controls* working_afa_ctrls_copy{ nullptr };  // Idc if this is a memory leak (raw pointer).  -Thea 2025/08/30
    std::string working_model_name{ "" };
    component_internal::Model_animator* working_model_animator{ nullptr };

    bool request_reset_agent_model{ true };
    std::map<std::string, size_t> anim_state_name_to_idx_map;
    size_t selected_anim_state_idx{ 0 };
    size_t selected_action_timeline_idx{ 0 };
    size_t selected_anim_num_frames{ 0 };
    size_t anim_current_frame{ 0 };
    bool is_working_afa_dirty{ false };
};

extern Editor_state s_editor_state;

void reset_editor_state();

}  // namespace anim_frame_action
}  // namespace TXP
