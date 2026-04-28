#pragma once

#include "btjson.h"
#include "txp_renderer/animation_driven_hitcapsule/hitcapsule.h"  // for `Hitcapsule_group_set`

#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace anim_frame_action
{

// Data controls.
struct Runtime_data_controls
{
    Runtime_data_controls(std::string const& fname);

    // Model const* animated_model{ nullptr };  @TODO figure this out

    struct Data
    {
        std::string animated_model_name;

        /// List of jump queues available for this AFA.
        struct Animation_state_set_jump_queues
        {
            std::string name;
            bool default_is_watching;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Animation_state_set_jump_queues,
                                           name,
                                           default_is_watching);
        };
        std::vector<Animation_state_set_jump_queues> anim_state_set_jump_queues;

        /// A timeline is paired with an animation state from the .btanitor file.
        struct Animation_frame_action_timeline
        {
            /// Multiple regions make up an action timeline. A region has a command run when the
            /// executing frame is within bounds.
            struct Region
            {
                uint32_t row_idx;
                int32_t  start_frame;
                int32_t  end_frame;

                /// The command run when the region is active. There are `on_first_frame` and
                /// `on_last_frame` flags.
                struct Control_command
                {
                    std::string cmd_name;
                    std::vector<std::string> argv;

                    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Control_command, cmd_name, argv);
                } ctrl_cmd;

                NLOHMANN_DEFINE_TYPE_INTRUSIVE(Region, row_idx, start_frame, end_frame, ctrl_cmd);
            };
            std::vector<Region> regions;
            std::string state_name;  // The corresponding animator state name this timeline belongs to.

            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Animation_frame_action_timeline, regions, state_name);
        };
        std::vector<Animation_frame_action_timeline> anim_frame_action_timelines;  // Same order as `model_animations`. (@CHECK: I think this is not true anymore)

        Hitcapsule_group_set hitcapsule_group_set_template;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Data,
                                       animated_model_name,
                                       anim_state_set_jump_queues,
                                       anim_frame_action_timelines,
                                       hitcapsule_group_set_template);
    } data;
};

// Bank of data controls.
class Bank
{
public:
    // @TODO: change this bank system to the interface of `Animator_template_bank` at some point.  -Thea 2026/03/31
    static void emplace(std::string const& name, Runtime_data_controls&& runtime_state);
    static void replace(std::string const& name, Runtime_data_controls&& runtime_state);
    static bool has(std::string const& name);
    static Runtime_data_controls const& get(std::string const& name);
    static std::vector<std::string> get_all_names();

private:
    inline static std::unordered_map<std::string, Runtime_data_controls> s_runtime_states;
};

}  // namespace anim_frame_action
}  // namespace TXP
