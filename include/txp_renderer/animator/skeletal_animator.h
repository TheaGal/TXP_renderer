#pragma once

#include "btglm.h"
#include "txp_renderer/animation_frame_action/runtime_data.h"
#include "txp_renderer/types.h"

#include <cmath>
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
    /// Sets a variable inside the state machine.
    void set_float_variable(std::string const& var_name, float_t value);


    /// Adds a state set to a jump queue.
    void emplace_jump_queue_state_set(std::string const& jump_queue_name,
                                      Animator_state_set const& state_set,
                                      float_t queue_expire_time);


    uint32_t get_animator_state_idx(std::string const& state_name) const;

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

private:
    component_internal::Model_animator* m_animator;

    /// Private ctor.
    Skeletal_animator(component_internal::Model_animator* internal_animator);  // @TODO: add reference counting for this!!! (pay attention to move ctors and dtors, or delete move/copy ctors).

    friend class Renderer;
};

}  // namespace TXP
