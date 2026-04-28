// clang-format off
#include "txp_renderer/animator/skeletal_animator.h"
// clang-format on

#include "txp_renderer/animation_frame_action/runtime_data.h"
#include "skeletal_animator.h"


namespace TXP
{

void Skeletal_animator::set_float_variable(std::string const& var_name, float_t value)
{
    m_animator->set_float_variable(var_name, value);
}


void Skeletal_animator::emplace_jump_queue_state_set(std::string const& jump_queue_name,
                                                     Animator_state_set const& state_set,
                                                     float_t queue_expire_time)
{
    m_animator->emplace_jump_queue_state_set(jump_queue_name, state_set, queue_expire_time);
}


uint32_t Skeletal_animator::get_animator_state_idx(std::string const& state_name) const
{
    return m_animator->get_animator_state_idx(state_name);
}

void Skeletal_animator::update(Animator_timer_profile profile, float_t delta_time)
{
    m_animator->update(profile, delta_time);
}


bool Skeletal_animator::get_is_using_root_motion() const
{
    return m_animator->get_is_using_root_motion();
}

void Skeletal_animator::get_anim_root_motion_delta_pos(Animator_timer_profile profile,
                                                       vec3& out_root_motion_delta_pos) const
{
    m_animator->get_anim_root_motion_delta_pos(profile, out_root_motion_delta_pos);
}

anim_frame_action::Runtime_controllable_data& Skeletal_animator::get_anim_frame_action_data_handle()
{
    return m_animator->get_anim_frame_action_data_handle();
}

Skeletal_animator::Skeletal_animator(component_internal::Model_animator* internal_animator)
    : m_animator(internal_animator)
{
}


}  // namespace TXP

