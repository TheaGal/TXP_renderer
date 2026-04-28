#pragma once

#include "animation_frame_action/runtime_data_controls.h"
#include "animator_template_types.h"
#include "btglm.h"
#include "btuuid.h"
#include "deformed_render_model.h"
#include "skeletal_animation.h"
#include "txp_renderer/animation_frame_action/runtime_data.h"
#include "txp_renderer/types.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

struct Deformed_model_data_set;  // Forward decl.

namespace component_internal
{

/// Internal unserializable component for render object animator.
class Model_animator  // @TODO: rename to `Skeletal_animator_internal`.
{
public:
    Model_animator(Deformed_model_animation_set const& model_anim_set,
                   Deformed_model_data_set const& deformed_model,
                   bool use_root_motion);

    Deformed_model_data_set& get_deformed_model() const;
    Deformed_model_skin const& get_model_skin() const;

    void configure_animator_states(
        std::vector<anim_tmpl_types::Animator_state> animator_states,
        std::vector<anim_tmpl_types::Animator_variable> animator_variables);

    /// Information to create a jump queue.
    struct Jump_queue_create
    {
        std::string queue_name;
        bool default_is_watching;
    };

    static std::vector<Jump_queue_create>
    make_jump_queue_create_list_from_anim_frame_action_controls(
        anim_frame_action::Runtime_data_controls const& anim_frame_action_controls);

    void configure_anim_frame_action_controls(
        anim_frame_action::Runtime_data_controls const* anim_frame_action_controls,
        BT::UUID resp_entity_uuid,
        std::vector<Jump_queue_create> const& jump_queues);

    std::vector<anim_tmpl_types::Animator_state> const& get_animator_states() const;
    uint32_t get_animator_state_idx(std::string const& state_name) const;
    anim_tmpl_types::Animator_state const& get_animator_state(size_t idx) const;
    anim_tmpl_types::Animator_state& get_animator_state_write_handle(size_t idx);

    /// Changes state-set.
    /// @note Public for being able to manually change state-sets instead of thru watching
    ///       jump-queues.
    void change_state_set(Animator_state_set const& to_state_set);

    size_t get_model_animation_idx(std::string anim_name) const;
    Model_joint_animation const& get_model_animation(size_t idx) const;

    // @NOTE: vv BELOW vv it really seems like at least float vars are still needed for specifically
    //   blend trees. This functionality will be kept for now, however, it might become a good idea
    //   to rework this into a better system or de-abstract the `Animator_variable` system.
    //     -Thea 2026/01/22
    //
    // @REF: see commit 3695cfba4873b8551ab404f40ff47eabc62b5700 for when other data type set/get
    //   funcs were deleted (e.g. `set_int_variable()`).
    //     -Thea 2026/01/22

    /// Sets a variable inside the state machine.
    void set_float_variable(std::string const& var_name, float_t value);

    /// Gets a variable inside the state machine.
    float_t get_float_variable(std::string const& var_name) const;

    /// Resets timers for all timer profiles of the animator.
    void reset_time();

    /// Sets timers for all timer profiles of the animator.
    void set_time(float_t time);

    /// Sets whether animator is paused.
    void set_paused(bool paused);

    /// Updates the animator, supplying a deltatime.
    /// There are two animator timers, so you need to give which timer to update.
    void update(Animator_timer_profile profile, float_t delta_time);

    /// Calculates the set of joint matrices, interpolated.
    /// Also allows for root motion zeroing.
    void calc_anim_pose(Animator_timer_profile profile,
                        bool root_motion_zeroing,
                        std::vector<mat4s>& out_joint_matrices) const;

    /// Gets whether root motion is enabled or not on this animator.
    bool get_is_using_root_motion() const;

    /// Calculates the set of joint matrices, floored. Note this one will be faster.
    /// Also allows for root motion zeroing.
    void get_anim_floored_frame_pose(Animator_timer_profile profile,
                                     bool root_motion_zeroing,
                                     std::vector<mat4s>& out_joint_matrices) const;

    /// Gets the root motion delta pos of the current frame.
    void get_anim_root_motion_delta_pos(Animator_timer_profile profile,
                                        vec3& out_root_motion_delta_pos) const;

    /// Gets reference to AFA (animation frame action) data.
    anim_frame_action::Runtime_controllable_data& get_anim_frame_action_data_handle();

    /// Documentation type for a control command.
    struct Ctrl_cmd_documentation
    {
        struct Name_w_desc
        {
            std::string name;
            std::string desc;
        } cmd;

        struct Name_w_desc_w_type
        {
            std::string name;
            std::string desc;
            std::string type;
        };
        std::vector<Name_w_desc_w_type> argv;

        std::function<void(Model_animator&, uint32_t, bool, bool, std::vector<std::string> const&)>
            exec_fn;
    };

    /// Gets documentation for all control cmds.
    static std::vector<Ctrl_cmd_documentation> const& get_control_command_codes_documentation();

    /// Accumulates delta time to update a timer for queue item expiring (runs in simulation loop).
    static void advance_sim_timer(float_t delta_time);

    /// Adds a state set to a jump queue.
    void emplace_jump_queue_state_set(std::string const& jump_queue_name,
                                      Animator_state_set const& state_set,
                                      float_t queue_expire_time);

    /// Resets jump queue watchlist to default values.
    void reset_jump_queue_watchlist();

    /// Sets whether watching a jump queue.
    /// Returns true if flag was changed, false if the flag was already set to that.
    bool set_watch_jump_queue(std::string const& jump_queue_name, bool watch, uint32_t priority);

    /// Fetches/pops first top priority state-set from set of watching jump queues.
    std::optional<Animator_state_set> pop_one_state_set();

private:
    Deformed_model_animation_set const& m_model_anim_set;
    Deformed_model_data_set const& m_deformed_model;

    // @NOTE: Times need to be atomic since `change_state_idx()` and `set_time()` can be called from
    //        any thread.
    using animator_time_t = typename std::atomic<float_t>;
    using animator_frame_t = typename std::atomic_uint32_t;

    animator_time_t& get_profile_time_handle(Animator_timer_profile profile) const;

    animator_time_t m_sim_time{ -1.0f };  // -1 for showing timer is unset on first update().
    animator_time_t m_rend_time{ -1.0f };

    animator_frame_t& get_profile_prev_frame_handle(Animator_timer_profile profile) const;

    animator_frame_t m_sim_prev_frame{ (uint32_t)-1 };  // -1 means unset.

    std::atomic_bool m_is_paused{ false };

    bool m_is_using_root_motion;

    /// Type for interpreted code.
    using cmd_code_t = anim_frame_action::Runtime_data_controls::Data::
        Animation_frame_action_timeline::Region::Control_command;

    /// Interprets and executes sent command code.
    void execute_command_code(cmd_code_t const& cmd_code,
                              uint32_t row_idx,
                              bool is_reg_first_frame,
                              bool is_reg_last_frame);

    ///////////////////////////////////////////////////

    std::vector<anim_tmpl_types::Animator_state> m_animator_states;
    std::vector<anim_tmpl_types::Animator_variable> m_animator_variables;

    struct Jump_queue_data
    {
        bool is_watching;
        bool default_is_watching;
        uint32_t priority;
        uint32_t default_priority;

        struct State_set_queue_item
        {
            Animator_state_set state_set;
            double_t queue_expire_time_absolute;
        };
        std::vector<State_set_queue_item> state_set_queue;
    };
    std::unordered_map<std::string, Jump_queue_data> m_jump_queue_name_to_jump_queue_map;

    struct State_set_runtime_data
    {
        mutable std::mutex mutex;
        Animator_state_set state_set;
    } m_current_state_set;
    std::atomic_uint32_t m_current_state_set_state_idx{ 0 };

    bool change_state_set_state_idx_goto_next(bool reset_count);

    struct Pair_state_set_info
    {
        uint32_t animator_state_idx;
        bool loop;
    };
    Pair_state_set_info get_animator_state_info_from_current_state_set() const;

    anim_frame_action::Runtime_data_controls const* m_anim_frame_action_controls{ nullptr };
    anim_frame_action::Runtime_controllable_data m_anim_frame_action_data;

    anim_tmpl_types::Animator_variable& find_animator_variable(std::string const& var_name);
    anim_tmpl_types::Animator_variable const& find_animator_variable_const(std::string const& var_name) const;

    struct Blend_value_result
    {
        uint32_t anim_idx_a;
        uint32_t anim_idx_b;
        float_t blend_t;
    };
    Blend_value_result calc_blend_value_of_blendtree(
        anim_tmpl_types::Animator_state const& anim_state) const;
};

}  // namespace component_internal
}  // namespace TXP
