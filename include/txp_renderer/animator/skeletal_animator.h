#pragma once

#include "btglm.h"
#include "txp_renderer/animation_frame_action/runtime_data.h"
#include "txp_renderer/types.h"

#include <cmath>
#include <functional>
#include <string>


namespace TXP
{

namespace component_internal
{
class Model_animator;  // Forward decl (not available externally).
}  // namespace component_internal

/// Externally facing skeletal animator.
class Skeletal_animator
{
public:
    using Play_audio_at_pos_fn_t =
        std::function<void(std::string const& snd_name, vec3 const pos, float_t volume)>;

    /// Sets callback func.
    static void set_play_audio_at_pos_oneshot_fn_callback(Play_audio_at_pos_fn_t&& callback_fn);


    /// Sets a variable inside the state machine.
    void set_float_variable(std::string const& var_name, float_t value);


    /// Adds an event to an event queue.
    void emplace_event(std::string const& event_queue_name, float_t queue_expire_time, int32_t arg);


    /// Updates the animator, supplying a deltatime.
    /// There are two animator timers, so you need to give which timer to update.
    void update(Animator_timer_profile profile, float_t delta_time);


    /// Gets whether root motion is enabled or not on this animator.
    bool get_is_using_root_motion() const;

    /// Gets the root motion delta pos of the current frame.
    void get_anim_root_motion_delta_pos(Animator_timer_profile profile,
                                        vec3& out_root_motion_delta_pos) const;

    /// Gets reference to AFA (animation frame action) data.
    anim_frame_action::Runtime_controllable_data& get_anim_frame_action_data_handle();

    /// Calculates set of joint matrices for simulation profile.
    void get_simulation_profile_frame_pose(bool root_motion_zeroing,
                                           std::vector<mat4s>& out_joint_matrices) const;

    /// Gets joint index in animator skin from name.
    uint32_t get_joint_idx(std::string const& joint_name) const;

private:
    inline static Play_audio_at_pos_fn_t s_play_audio_at_pos_oneshot_fn{ nullptr };

    component_internal::Model_animator* m_animator;

    /// Private ctor.
    Skeletal_animator(component_internal::Model_animator* internal_animator);  // @TODO: add reference counting for this!!! (pay attention to move ctors and dtors, or delete move/copy ctors).

    friend class Renderer;
    friend class component_internal::Model_animator;
};

}  // namespace TXP
