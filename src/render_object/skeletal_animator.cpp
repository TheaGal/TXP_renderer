#include "skeletal_animator.h"

#include "animation_frame_action/runtime_data.h"
#include "animator_template_types.h"
#include "btglm.h"
#include "btlogger.h"
#include "btuuid.h"
// #include "mesh.h"  @TODO
#include "txp_renderer/types.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>


TXP::Model_joint_animation_frame::Joint_local_transform
TXP::Model_joint_animation_frame::Joint_local_transform::interpolate_fast(
    Joint_local_transform const& other,
    float_t t) const
{
    Joint_local_transform ret_trans;
#if 0  /* I removed the `STEP` interpolation type since I figured that at least for skeletal animations I'm not gonna include it  -Thea 2025/07/13 */
    switch (interp_type)
    {
        case INTERP_TYPE_LINEAR:
#endif  // 0
            glm_vec3_lerp(const_cast<float_t*>(position),
                          const_cast<float_t*>(other.position),
                          t,
                          ret_trans.position);
            glm_quat_nlerp(const_cast<float_t*>(rotation),
                           const_cast<float_t*>(other.rotation),
                           t,
                           ret_trans.rotation);
            glm_vec3_lerp(const_cast<float_t*>(scale),
                          const_cast<float_t*>(other.scale),
                          t,
                          ret_trans.scale);
#if 0
            ret_trans.interp_type = INTERP_TYPE_LINEAR;
            break;

        case INTERP_TYPE_STEP:
            glm_vec3_copy(const_cast<float_t*>(position),
                          ret_trans.position);
            glm_quat_copy(const_cast<float_t*>(rotation),
                          ret_trans.rotation);
            glm_vec3_copy(const_cast<float_t*>(scale),
                          ret_trans.scale);
            ret_trans.interp_type = INTERP_TYPE_STEP;
            break;

        default:
            assert(false);
            break;
    }
#endif  // 0
    return ret_trans;
}


TXP::Model_joint_animation::Model_joint_animation(
    Deformed_model_skin const& skin,
    std::string name,
    std::vector<Model_joint_animation_frame>&& animation_frames)
    : m_model_skin{ skin }
    , m_name{ name }
    , m_frames{ std::move(animation_frames) }
{
}

uint32_t TXP::Model_joint_animation::calc_frame_idx(float_t time,
                                                    bool loop,
                                                    Rounding_func rounding) const
{
    assert(time >= 0.0f);
    assert(m_frames.size() >= 1);

    uint32_t frame_idx;
    switch (rounding)
    {
        case FLOOR:
            frame_idx = std::floor(time * k_skeletal_anim_frames_per_second);
            break;

        case CEIL:
            frame_idx = std::ceil(time * k_skeletal_anim_frames_per_second);
            break;

        default:
            // Incorrect value entered.
            assert(false);
            break;
    }

    if (loop)
    {
        // Loop keyframes.
        frame_idx = (frame_idx % m_frames.size());
    }
    else
    {
        // Clamp keyframes to base frame.
        frame_idx = std::min(frame_idx, static_cast<uint32_t>(m_frames.size()) - 1);
    }

    return frame_idx;
}

TXP::Model_joint_animation::Joint_local_transform_set_t
TXP::Model_joint_animation::calc_joint_local_transforms_interpolated(float_t time,
                                                                     bool loop,
                                                                     bool root_motion_zeroing) const
{
    uint32_t frame_idx_a{ calc_frame_idx(time, loop, FLOOR) };
    uint32_t frame_idx_b{ calc_frame_idx(time, loop, CEIL) };

    assert(m_model_skin.joints_sorted_breadth_first.size() ==
           m_frames[frame_idx_a].joint_transforms_in_order.size());
    assert(m_model_skin.joints_sorted_breadth_first.size() ==
           m_frames[frame_idx_b].joint_transforms_in_order.size());

    float_t interp_t{ (time / k_skeletal_anim_frames_per_second)
                      - std::floor(time / k_skeletal_anim_frames_per_second) };

    Joint_local_transform_set_t local_joint_transforms;
    local_joint_transforms.reserve(m_model_skin.joints_sorted_breadth_first.size());

    // Calculate joint transforms.
    for (size_t i = 0; i < m_model_skin.joints_sorted_breadth_first.size(); i++)
    {   // Calculate local transform.
        local_joint_transforms.emplace_back(
            m_frames[frame_idx_a].joint_transforms_in_order[i].interpolate_fast(
                m_frames[frame_idx_b].joint_transforms_in_order[i],
                interp_t));

        if (i == 0 && root_motion_zeroing)
        {   // Delete root motion (for XZ axes).
            local_joint_transforms.back().position[0] = local_joint_transforms.back().position[2] = 0;
        }
    }

    return local_joint_transforms;
}

TXP::Model_joint_animation::Joint_local_transform_set_t
TXP::Model_joint_animation::calc_joint_local_transforms_floored(float_t time,
                                                                bool loop,
                                                                bool root_motion_zeroing) const
{
    uint32_t frame_idx{ calc_frame_idx(time, loop, FLOOR) };

    assert(m_model_skin.joints_sorted_breadth_first.size() ==
           m_frames[frame_idx].joint_transforms_in_order.size());

    Joint_local_transform_set_t local_joint_transforms;
    local_joint_transforms.reserve(m_model_skin.joints_sorted_breadth_first.size());

    // Calculate joint transforms.
    for (size_t i = 0; i < m_model_skin.joints_sorted_breadth_first.size(); i++)
    {   // Calculate local transform.
        local_joint_transforms.emplace_back(m_frames[frame_idx].joint_transforms_in_order[i]);

        if (i == 0 && root_motion_zeroing)
        {   // Delete root motion (for XZ axes).
            local_joint_transforms.back().position[0] = local_joint_transforms.back().position[2] = 0;
        }
    }

    return local_joint_transforms;
}

TXP::Model_joint_animation::Joint_local_transform_set_t
TXP::Model_joint_animation::blend_joint_local_transform_sets(Joint_local_transform_set_t const& a,
                                                             Joint_local_transform_set_t const& b,
                                                             float_t blend_t)
{
    assert(a.size() == b.size());

    Joint_local_transform_set_t local_joint_transforms;
    local_joint_transforms.reserve(a.size());

    for (size_t i = 0; i < a.size(); i++)
    {
        local_joint_transforms.emplace_back(a[i].interpolate_fast(b[i], blend_t));
    }

    return local_joint_transforms;
}

void TXP::Model_joint_animation::calc_joint_matrices(
    Joint_local_transform_set_t const& joint_local_transforms,
    std::vector<mat4s>& out_joint_matrices) const
{
    assert(m_model_skin.joints_sorted_breadth_first.size() == joint_local_transforms.size());

    // Allocate calculation cache.
    std::vector<mat4s> joint_global_transform_cache;
    joint_global_transform_cache.resize(m_model_skin.joints_sorted_breadth_first.size());

    out_joint_matrices.resize(m_model_skin.joints_sorted_breadth_first.size());

    // Calculate joint matrices.
    for (size_t i = 0; i < m_model_skin.joints_sorted_breadth_first.size(); i++)
    {
        auto const& joint{ m_model_skin.joints_sorted_breadth_first[i] };
        if (i == 0 && joint.parent_idx != (uint32_t)-1)
        {
            BT_ERROR(
                "First joint\'s parent is not null. Joint list probably not sorted. Aborting.");
            assert(false);
            return;
        }

        // Calculate global transform (relative to parent bone -> model space).
        auto const& local_joint_transform{ joint_local_transforms[i] };

        mat4 global_joint_transform;
        glm_translate_make(global_joint_transform,
                           const_cast<float_t*>(local_joint_transform.position));
        glm_quat_rotate(global_joint_transform,
                        const_cast<float_t*>(local_joint_transform.rotation),
                        global_joint_transform);
        glm_scale(global_joint_transform, const_cast<float_t*>(local_joint_transform.scale));

        if (joint.parent_idx == (uint32_t)-1)
        {   // Use skin baseline transform.
            glm_mat4_mul(const_cast<vec4*>(m_model_skin.baseline_transform),
                 global_joint_transform,
                 global_joint_transform);
        }
        else
        {   // Use cached parent global trans to make global trans.
            glm_mat4_mul(joint_global_transform_cache[joint.parent_idx].raw,
                         global_joint_transform,
                         global_joint_transform);
        }

        // Insert global transform into cache.
        glm_mat4_copy(global_joint_transform, joint_global_transform_cache[i].raw);

        // Calculate joint matrix.
        // @RANT: I hate how all the glm functions don't mark the params as const,
        //   and also since they're not getting mutated! Aaaaggghhhh
        // @RANT: I hate how the rant above was a rant!!! The amount of strenuous
        //   work to get this whole shitshow working was insane!!!! Hahahahahahaha  -Thea 2025/07/20
        mat4 joint_matrix;
        glm_mat4_mul(const_cast<vec4*>(m_model_skin.inverse_global_transform),
                     global_joint_transform,
                     joint_matrix);
        glm_mat4_mul(joint_matrix,
                     const_cast<vec4*>(joint.inverse_bind_matrix),
                     out_joint_matrices[i].raw);
    }
}

void TXP::Model_joint_animation::get_root_motion_delta_pos_at_frame(
    uint32_t frame_idx,
    vec3& out_root_motion_delta_pos) const
{   // Include root motion delta position (Just XZ axes).
    glm_vec3_copy(const_cast<float_t*>(m_frames[frame_idx].root_motion_delta_pos),
                  out_root_motion_delta_pos);
    out_root_motion_delta_pos[1] = 0;
}


TXP::Model_animator::Model_animator(Deformed_model_animation_set const& model_anim_set,
                                    Deformed_model_skin const& model_skin,
                                    bool use_root_motion)
    : m_model_anim_set{ model_anim_set }
    , m_model_skin{ model_skin }
    , m_is_using_root_motion{ use_root_motion }
{
}

TXP::Deformed_model_skin const& TXP::Model_animator::get_model_skin() const
{
    return m_model_skin;
}

void TXP::Model_animator::configure_animator_states(
    std::vector<anim_tmpl_types::Animator_state> animator_states,
    std::vector<anim_tmpl_types::Animator_variable> animator_variables)
{
    // Idk why I put this into a separate method instead of in the constructor but hey, here we are.
    // @NOTE: A lot of copying, but I'm @TEMP temporarily doing this for a looser interface.
    m_animator_states = animator_states;
    m_animator_variables = animator_variables;
}

std::vector<TXP::Model_animator::Jump_queue_create> TXP::Model_animator::
    make_jump_queue_create_list_from_anim_frame_action_controls(
        anim_frame_action::Runtime_data_controls const& anim_frame_action_controls)
{
    std::vector<Jump_queue_create> jqc;
    jqc.reserve(anim_frame_action_controls.data.anim_state_set_jump_queues.size());

    for (auto const& jq : anim_frame_action_controls.data.anim_state_set_jump_queues)
    {
        jqc.emplace_back(jq.name, jq.default_is_watching);
    }

    return jqc;
}

void TXP::Model_animator::configure_anim_frame_action_controls(
    anim_frame_action::Runtime_data_controls const* anim_frame_action_controls,
    BT::UUID resp_entity_uuid,
    std::vector<Jump_queue_create> const& jump_queues)
{
    // Idk why I put this into a separate method instead of in the constructor but hey, here we are.
    m_anim_frame_action_controls = anim_frame_action_controls;

    m_anim_frame_action_data.map_animator_to_control_regions(*this, *m_anim_frame_action_controls);

    m_anim_frame_action_data.hitcapsule_group_set.replace_and_reregister(
        m_anim_frame_action_controls->data.hitcapsule_group_set_template,
        resp_entity_uuid);
    m_anim_frame_action_data.hitcapsule_group_set.connect_animator(*this);
    
    // Insert jump queues.
    // @NOTE: if `jump_queues` is empty, read from the AFA ctrl jump queue.
    assert(m_jump_queue_name_to_jump_queue_map.empty());

    uint32_t next_def_watch_priority{ 0x80000000 };  // Place default-watching queues at least this low of priority.
    for (auto const& jq : jump_queues)
    {
        uint32_t default_priority{ jq.default_is_watching ? next_def_watch_priority++
                                                          : (uint32_t)-1 };

        m_jump_queue_name_to_jump_queue_map.emplace(
            jq.queue_name,
            Jump_queue_data{
                .is_watching = jq.default_is_watching,
                .default_is_watching = jq.default_is_watching,
                .priority = default_priority,
                .default_priority = default_priority,
            });
    }
}

std::vector<TXP::anim_tmpl_types::Animator_state> const&
TXP::Model_animator::get_animator_states() const
{
    return m_animator_states;
}

uint32_t TXP::Model_animator::get_animator_state_idx(std::string const& state_name) const
{
    uint32_t idx{ (uint32_t)-1 };

    for (uint32_t i = 0; i < m_animator_states.size(); i++)
        if (m_animator_states[i].state_name == state_name)
        {   // Found name.
            idx = i;
            break;
        }

    if (idx == (uint32_t)-1)
    {
        BT_ERRORF(
            "Could not find animator state \"%s\". Aborting program.",
            state_name.c_str());
        abort();
    }

    return idx;
}

TXP::anim_tmpl_types::Animator_state const&
TXP::Model_animator::get_animator_state(size_t idx) const
{
    return m_animator_states[idx];
}

TXP::anim_tmpl_types::Animator_state&
TXP::Model_animator::get_animator_state_write_handle(size_t idx)
{
    return m_animator_states[idx];
}

void TXP::Model_animator::change_state_set(Animator_state_set const& to_state_set)
{
    if (to_state_set.anim_state_indices.empty())
    {
        BT_ERROR("Supplied state set number of states is 0.");
        assert(false);
        return;
    }
    {
        std::lock_guard<std::mutex> lock{ m_current_state_set.mutex };
        m_current_state_set.state_set = to_state_set;  // Make copy of state set.

        BT_WARNF("Inserted state-set (loop_final=%u):",
                  m_current_state_set.state_set.loop_final_state);

        for (size_t i = 0; i < m_current_state_set.state_set.anim_state_indices.size(); i++)
        {
            auto state_idx = m_current_state_set.state_set.anim_state_indices[i];
            BT_WARNF("   state-idx[%llu]=%u", i, state_idx);
        }
    }

    (void)change_state_set_state_idx_goto_next(true);
}

size_t TXP::Model_animator::get_model_animation_idx(std::string anim_name) const
{
    size_t idx{ (size_t)-1 };

    for (size_t i = 0; i < m_model_anim_set.animations.size(); i++)
        if (m_model_anim_set.animations[i].get_name() == anim_name)
        {   // Found name.
            idx = i;
            break;
        }

    if (idx == (size_t)-1)
    {
        BT_ERRORF(
            "Could not find model animation \"%s\" inside animator copied set of animations from "
            "the model. Aborting program.",
            anim_name.c_str());
        abort();
    }

    return idx;
}

TXP::Model_joint_animation const& TXP::Model_animator::get_model_animation(size_t idx) const
{
    return m_model_anim_set.animations[idx];
}

void TXP::Model_animator::set_float_variable(std::string const& var_name, float_t value)
{
    auto& var_handle{ find_animator_variable(var_name) };

    if (var_handle.type != anim_tmpl_types::Animator_variable::TYPE_FLOAT)
    {
        assert(false);
        return;
    }

    var_handle.var_value = value;
}

float_t TXP::Model_animator::get_float_variable(std::string const& var_name) const
{
    auto const& var_handle{ find_animator_variable_const(var_name) };

    if (var_handle.type != anim_tmpl_types::Animator_variable::TYPE_FLOAT)
    {
        assert(false);
        return std::numeric_limits<float_t>::lowest();
    }

    return var_handle.var_value;
}

void TXP::Model_animator::reset_time()
{
    set_time(0.0f);
    m_sim_prev_frame = (uint32_t)-1;
}

void TXP::Model_animator::set_time(float_t time)
{
    // @NOTE: since SIMULATION_PROFILE floors for calc frame idx, set to start at 1/2 one frame to
    //        prevent floating-pt error accumulation.  -Thea 2026/01/17
    constexpr float_t k_half_frame_offset{ 0.5f / k_skeletal_anim_frames_per_second };

    auto time_floored_to_frame{ std::floor(time * k_skeletal_anim_frames_per_second) /
                                k_skeletal_anim_frames_per_second };

    m_sim_time  = time_floored_to_frame + k_half_frame_offset;
    m_rend_time = time;
}

void TXP::Model_animator::set_paused(bool paused)
{
    m_is_paused.store(paused);
}

void TXP::Model_animator::update(Animator_timer_profile profile, float_t delta_time)
{   // @TODO: There needs to be some kind of time syncing between timers. Since the creation of
    //        setting triggers and variables to switch states, there will be issues when changing
    //        states.
    // @THOUGHT: Well, ig since `set_time()` will be setting all the timers, then it will start out
    //           synced up enough? Only the simulation loop is going to be changing states inside
    //           the animator.

    animator_time_t& time_handle{ get_profile_time_handle(profile) };
    auto [anim_state_idx, anim_loop]{ get_animator_state_info_from_current_state_set() };
    auto const& anim_state{ m_animator_states[anim_state_idx] };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Process anim frame action controls.
    if (profile == SIMULATION_PROFILE &&
        m_anim_frame_action_controls != nullptr)
    {
        // Process anim frame action runtime.
        auto current_action_timeline_idx{
            m_anim_frame_action_data.anim_state_idx_to_timeline_idx_map.at(anim_state_idx)
        };
        auto& afa_timeline{ m_anim_frame_action_controls->data
                                .anim_frame_action_timelines[current_action_timeline_idx] };

        m_anim_frame_action_data.clear_all_data_overrides();

        // Copy current and previous times.
        float_t curr_time{ time_handle };

        // Get anim frame idx.
        auto const& anim_state{ m_animator_states[anim_state_idx] };
        auto const& current_anim_for_state{
            m_model_anim_set.animations[anim_state.state_type == anim_state.SINGLE_ANIM
                                            ? anim_state.animation_idx
                                            : anim_state.blend_anims.front().animation_idx]
        };
        auto frame_idx{ current_anim_for_state.calc_frame_idx(curr_time,
                                                              anim_loop,
                                                              Model_joint_animation::FLOOR) };
        auto num_frames{ current_anim_for_state.get_num_frames() };

        // Get prev anim frame idx.
        animator_frame_t& prev_frame_handle{ get_profile_prev_frame_handle(profile) };
        uint32_t prev_frame_idx{ prev_frame_handle };

        // Test to make sure that frames only advance one at a time.
        // Test case: ensure that
        //     (1) the prev frame is unset and the current frame is 0, or
        //     (2) the prev frame is the last frame of a looping anim and the current frame is 0, or
        //     (3) the prev frame and current frame are both at the last frame of non-looped anim, or
        //     (4) the prev frame is exactly 1 behind the current frame.
        if (!((prev_frame_idx == (uint32_t)-1 && frame_idx == 0) ||
              (anim_loop && prev_frame_idx + 1 == num_frames && frame_idx == 0) ||
              (!anim_loop && prev_frame_idx + 1 == num_frames && frame_idx + 1 == num_frames) ||
              (prev_frame_idx + 1 == frame_idx)))
        {
            BT_ERRORF(
                "Frame advanced in SIMULATION_PROFILE in an unusual way. prev_frame_idx=%u "
                "frame_idx=%u num_frames=%u",
                prev_frame_idx,
                frame_idx,
                num_frames);
            assert(false);
        }

        // Process timeline regions.
        for (auto const& region : afa_timeline.regions)
        {
            // Check if within region.
            if (frame_idx >= region.start_frame && frame_idx < region.end_frame)
            {
                execute_command_code(region.ctrl_cmd,
                                     region.row_idx,
                                     frame_idx == region.start_frame,
                                     frame_idx == region.end_frame - 1);
            }
        }

        // Update prev frame.
        prev_frame_handle = frame_idx;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Process animator state transitions.
    bool performed_state_transition{ false };

    if (profile == SIMULATION_PROFILE)
    {
        bool state_set_changed{ false };

        // Get state-set transition from watching jump queues.
        auto trans_state_set{ pop_one_state_set() };

        // Perform actual state-set change!
        if (trans_state_set.has_value())
        {
            change_state_set(trans_state_set.value());
            state_set_changed = true;
            performed_state_transition = true;
        }

        // Check for end of anim case to move to next state idx.
        // @NOTE: lesser priority than state-set change.
        if (!state_set_changed)
        {
            auto const& model_anim{
                m_model_anim_set.animations[anim_state.state_type == anim_state.SINGLE_ANIM
                                                ? anim_state.animation_idx
                                                : anim_state.blend_anims.front().animation_idx]
            };

            bool is_at_last_frame{ model_anim.calc_frame_idx(time_handle.load(),
                                                             false,
                                                             Model_joint_animation::FLOOR) ==
                                   model_anim.get_num_frames() - 1 };
            if (is_at_last_frame)
            {
                auto success = change_state_set_state_idx_goto_next(false);
                if (success)
                {   // Performed a transition!
                    performed_state_transition = true;
                }
            }
        }

        // vv (@NOTE: currently @UNUSED) vv

        // Erase all trigger activations!
        for (auto& anim_var : m_animator_variables)
        if (anim_var.type == anim_tmpl_types::Animator_variable::TYPE_TRIGGER)
        {
            anim_var.var_value = 0;
        }

        // ^^ (@NOTE: currently @UNUSED) ^^
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Tick forward.
    // @NOTE: if just performed a state transition earlier,
    //        do *not* change time so that frame 0 isn't skipped.  -Thea 2026/01/24
    if (!m_is_paused.load() && !performed_state_transition)
    {
        time_handle += delta_time;

        // @NOTE: time dilation in the simulation profile is not allowed.
        if (profile == SIMULATION_PROFILE && delta_time != k_simulation_delta_time)
        {
            BT_ERRORF(
                "SIMULATION_PROFILE animator tick did not match simulation delta-time.  "
                "k_simulation_delta_time=%0.6f  delta_time=%0.6f",
                k_simulation_delta_time,
                delta_time);
            assert(false);
            abort();
            return;
        }
    }
}

void TXP::Model_animator::calc_anim_pose(Animator_timer_profile profile,
                                         bool root_motion_zeroing,
                                         std::vector<mat4s>& out_joint_matrices) const
{
    auto time{ get_profile_time_handle(profile).load() };
    auto [anim_state_idx, anim_loop]{ get_animator_state_info_from_current_state_set() };
    auto const& anim_state{ m_animator_states[anim_state_idx] };

    switch (anim_state.state_type)
    {
    case anim_tmpl_types::Animator_state::SINGLE_ANIM:
    {   // Get single anim pose.
        auto joint_local_transforms{
            m_model_anim_set.animations[anim_state.animation_idx]
                .calc_joint_local_transforms_interpolated(time,
                                                          anim_loop,
                                                          root_motion_zeroing)
        };
        m_model_anim_set.animations[anim_state.animation_idx].calc_joint_matrices(
            joint_local_transforms,
            out_joint_matrices);
        break;
    }

    case anim_tmpl_types::Animator_state::BLENDTREE:
    {
        auto [anim_idx_a, anim_idx_b, blend_t]{ calc_blend_value_of_blendtree(anim_state) };

        // Calc model animations of both anims.
        constexpr size_t k_num_sets{ 2 };
        Model_joint_animation::Joint_local_transform_set_t joint_trans_sets[k_num_sets];
        for (size_t i = 0; i < k_num_sets; i++)
        {
            auto anim_idx{ i == 0 ? anim_idx_a : anim_idx_b };

            joint_trans_sets[i] =
                m_model_anim_set.animations[anim_idx].calc_joint_local_transforms_interpolated(
                    time,
                    anim_loop,
                    root_motion_zeroing);
        }

        // Blend model anims.
        auto blended_trans_set{ Model_joint_animation::blend_joint_local_transform_sets(
            joint_trans_sets[0],
            joint_trans_sets[1],
            blend_t) };

        // @NOCHECKIN: @TODO: @THEA: CHANGE THIS TO BE IN `Model_animator` INSTEAD!!!!!
        // Because now which model animation runs this doesn't matter.
        m_model_anim_set.animations[0].calc_joint_matrices(blended_trans_set, out_joint_matrices);

        break;
    }

    default: assert(false); break;  // Unsupported type.
    }
}

bool TXP::Model_animator::get_is_using_root_motion() const
{
    return m_is_using_root_motion;
}

void TXP::Model_animator::get_anim_floored_frame_pose(Animator_timer_profile profile,
                                                      bool root_motion_zeroing,
                                                      std::vector<mat4s>& out_joint_matrices) const
{
    auto time{ get_profile_time_handle(profile).load() };
    auto [anim_state_idx, anim_loop]{ get_animator_state_info_from_current_state_set() };
    auto const& anim_state{ m_animator_states[anim_state_idx] };

    switch (anim_state.state_type)
    {
    case anim_tmpl_types::Animator_state::SINGLE_ANIM:
    {   // Get single anim pose.
        auto joint_local_transforms{
            m_model_anim_set.animations[anim_state.animation_idx].calc_joint_local_transforms_floored(
                time,
                anim_loop,
                root_motion_zeroing)
        };
        m_model_anim_set.animations[anim_state.animation_idx].calc_joint_matrices(
            joint_local_transforms,
            out_joint_matrices);
        break;
    }

    case anim_tmpl_types::Animator_state::BLENDTREE:
    {
        auto [anim_idx_a, anim_idx_b, blend_t]{ calc_blend_value_of_blendtree(anim_state) };

        // Calc model animations of both anims.
        constexpr size_t k_num_sets{ 2 };
        Model_joint_animation::Joint_local_transform_set_t joint_trans_sets[k_num_sets];
        for (size_t i = 0; i < k_num_sets; i++)
        {
            auto anim_idx{ i == 0 ? anim_idx_a : anim_idx_b };

            joint_trans_sets[i] =
                m_model_anim_set.animations[anim_idx].calc_joint_local_transforms_floored(
                    time,
                    anim_loop,
                    root_motion_zeroing);
        }

        // Blend model anims.
        auto blended_trans_set{ Model_joint_animation::blend_joint_local_transform_sets(
            joint_trans_sets[0],
            joint_trans_sets[1],
            blend_t) };

        // @NOCHECKIN: @TODO: @THEA: CHANGE THIS TO BE IN `Model_animator` INSTEAD!!!!!
        // Because now which model animation runs this doesn't matter.
        m_model_anim_set.animations[0].calc_joint_matrices(blended_trans_set, out_joint_matrices);

        break;
    }

    default: assert(false); break;  // Unsupported type.
    }
}

void TXP::Model_animator::get_anim_root_motion_delta_pos(Animator_timer_profile profile,
                                                         vec3& out_root_motion_delta_pos) const
{
    auto time{ get_profile_time_handle(profile).load() };
    auto [anim_state_idx, anim_loop]{ get_animator_state_info_from_current_state_set() };
    auto const& anim_state{ m_animator_states[anim_state_idx] };

    switch (anim_state.state_type)
    {
    case anim_tmpl_types::Animator_state::SINGLE_ANIM:
    {   // Get single anim root motion.
        uint32_t frame_idx{ m_model_anim_set.animations[anim_state.animation_idx].calc_frame_idx(
            time,
            anim_loop,
            Model_joint_animation::FLOOR) };
        m_model_anim_set.animations[anim_state.animation_idx].get_root_motion_delta_pos_at_frame(
            frame_idx,
            out_root_motion_delta_pos);
        break;
    }

    case anim_tmpl_types::Animator_state::BLENDTREE:
    {
        auto [anim_idx_a, anim_idx_b, blend_t]{ calc_blend_value_of_blendtree(anim_state) };
        
        // Get root motion of both anims.
        constexpr size_t k_num_root_motions{ 2 };
        vec3 root_motions[k_num_root_motions];
        for (size_t i = 0; i < k_num_root_motions; i++)
        {
            auto anim_idx{ i == 0 ? anim_idx_a : anim_idx_b };
            auto& root_motion_ref{ root_motions[i] };

            uint32_t frame_idx{ m_model_anim_set.animations[anim_idx].calc_frame_idx(
                time,
                anim_loop,
                Model_joint_animation::FLOOR) };
            m_model_anim_set.animations[anim_idx].get_root_motion_delta_pos_at_frame(
                frame_idx,
                root_motion_ref);
        }

        // Blend root motions (preserving magnitude).
        vec3 lerped_root_motion;
        glm_vec3_lerp(root_motions[0], root_motions[1], blend_t, lerped_root_motion);

        if (float_t lerped_rm_magnitude{ glm_vec3_norm(lerped_root_motion) };
            lerped_rm_magnitude > 1e-6f)
        {   // Only preserve magnitude if the lerped root motion vector is not essentially zero.
            float_t magnitude_0{ glm_vec3_norm(root_motions[0]) };
            float_t magnitude_1{ glm_vec3_norm(root_motions[1]) };
            float_t target_magnitude{ glm_lerp(magnitude_0, magnitude_1, blend_t) };

            glm_vec3_scale(lerped_root_motion,
                           target_magnitude / lerped_rm_magnitude,
                           lerped_root_motion);
        }

        glm_vec3_copy(lerped_root_motion, out_root_motion_delta_pos);

        break;
    }

    default: assert(false); break;  // Unsupported type.
    }
}

TXP::anim_frame_action::Runtime_controllable_data&
TXP::Model_animator::get_anim_frame_action_data_handle()
{
    return m_anim_frame_action_data;
}

std::vector<TXP::Model_animator::Ctrl_cmd_documentation> const& TXP::Model_animator::
    get_control_command_codes_documentation()
{
    static std::vector<Ctrl_cmd_documentation> const k_all_cmd_docs{
        {
            .cmd{
                .name = "nop",
                .desc = "No operation"
            },
            .argv{},
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                BT_WARN("nop() executed.");
            }
        },
        {
            .cmd{
                .name = "var_ov",
                .desc = "Sets a variable temporarily for the frame, effectively overriding the "
                        "default value for just the frame(s) this ctrl cmd is active."
            },
            .argv{
                {
                    .name = "var_name",
                    .desc = "Variable name to mutate",
                    .type = "str"
                },
                {
                    .name = "value",
                    .desc = "Value to set (will be casted depending on var-type)",
                    .type = "str"
                },
            },
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                auto& afa_data{ animator.m_anim_frame_action_data };
                auto label{ afa_data.str_label_to_enum(argv[0]) };
                switch (afa_data.get_data_type(label))
                {
                    using CD_t = TXP::anim_frame_action::Runtime_controllable_data::Controllable_data_type;

                    case CD_t::CTRL_DATA_TYPE_FLOAT:
                        afa_data.get_float_data_handle(label).override_val(std::stof(argv[1]));
                        break;

                    case CD_t::CTRL_DATA_TYPE_BOOL:
                        afa_data.get_bool_data_handle(label).override_val(argv[1] == "true");
                        break;

                    default: assert(false); break;
                }
            }
        },
        {
            .cmd{
                .name = "var_wr",
                .desc = "Sets a variable, writing it."
            },
            .argv{
                {
                    .name = "var_name",
                    .desc = "Variable name to mutate",
                    .type = "str"
                },
                {
                    .name = "value",
                    .desc = "Value to set (will be casted depending on var-type)",
                    .type = "str"
                },
            },
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                auto& afa_data{ animator.m_anim_frame_action_data };
                auto label{ afa_data.str_label_to_enum(argv[0]) };
                switch (afa_data.get_data_type(label))
                {
                    using CD_t = TXP::anim_frame_action::Runtime_controllable_data::Controllable_data_type;

                    case CD_t::CTRL_DATA_TYPE_FLOAT:
                        afa_data.get_float_data_handle(label).write_val(std::stof(argv[1]));
                        break;

                    case CD_t::CTRL_DATA_TYPE_BOOL:
                        afa_data.get_bool_data_handle(label).write_val(argv[1] == "true");
                        break;

                    default: assert(false); break;
                }
            }
        },
        {
            .cmd{
                .name = "var_mark_reeve",
                .desc = "Marks a rising-edge event."
            },
            .argv{
                {
                    .name = "var_name",
                    .desc = "Variable name to mark the rising-edge event of",
                    .type = "str"
                },
            },
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                if (!is_first_frame)
                    return;  // Exit early if not rising edge.

                auto& afa_data{ animator.m_anim_frame_action_data };
                auto label{ afa_data.str_label_to_enum(argv[0]) };
                switch (afa_data.get_data_type(label))
                {
                    using CD_t = TXP::anim_frame_action::Runtime_controllable_data::Controllable_data_type;

                    case CD_t::CTRL_DATA_TYPE_RISING_EDGE_EVENT:
                            afa_data.get_reeve_data_handle(label).mark_rising_edge();
                        break;

                    default: assert(false); break;
                }
            }
        },
        {
            .cmd{
                .name = "blend",
                .desc = "Blends current animation with previous one set as an anchor pose, lerping "
                        "from 0-1 over the cmd region."
            },
            .argv{},
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                BT_ERROR("NOT IMPLEMENTED YET: blend().");
                assert(false);
            }
        },
        {
            .cmd{
                .name = "watch_jump_queue",
                .desc = "Checks the specified animation state queue to see if an available state "
                        "exists, and if so, jumps to that animation state."
            },
            .argv{
                {
                    .name = "anim_state_queue",
                    .desc = "Queue to check for anim state queues this frame",
                    .type = "str"
                }
            },
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                // Add `anim_state_queue` for watching this frame.
                bool changed = animator.set_watch_jump_queue(argv[0], true, row_idx);
                if (!changed)
                    throw std::runtime_error("Jump queue is already set to the wanted way.");
            }
        },
        {
            .cmd{
                .name = "ignore_jump_queue",
                .desc = "Ignores the specified animation state queue to prevent it from jumping "
                        "anim states."
            },
            .argv{
                {
                    .name = "anim_state_queue",
                    .desc = "Queue to ignore this frame",
                    .type = "str"
                }
            },
            .exec_fn = [](Model_animator& animator,
                          uint32_t row_idx,
                          bool is_first_frame,
                          bool is_last_frame,
                          std::vector<std::string> const& argv) {
                // Remove `anim_state_queue` from watching for this frame.
                bool changed = animator.set_watch_jump_queue(argv[0], false, -1);
                if (!changed)
                    throw std::runtime_error("Jump queue is already set to the wanted way.");
            }
        },
    };

    return k_all_cmd_docs;
}

namespace
{
    static std::atomic<double_t> s_sim_timer{ 0 };
}

void TXP::Model_animator::advance_sim_timer(float_t delta_time)
{   // @NOTE: this does not pay attention to time-scale (if it is ever implemented) because
    //        `s_sim_timer` is used for expiring queue items, not for the actual animator.
    //          -Thea 2026/01/25
    s_sim_timer += delta_time;
}

void TXP::Model_animator::emplace_jump_queue_state_set(std::string const& jump_queue_name,
                                                       Animator_state_set const& state_set,
                                                       float_t queue_expire_time)
{
    m_jump_queue_name_to_jump_queue_map.at(jump_queue_name)
        .state_set_queue.emplace_back(state_set, s_sim_timer + queue_expire_time);
}

void TXP::Model_animator::reset_jump_queue_watchlist()
{   // Reset back to defaults.
    for (auto& [_, jq] : m_jump_queue_name_to_jump_queue_map)
    {
        jq.is_watching = jq.default_is_watching;
        jq.priority = jq.default_priority;
    }
}

bool TXP::Model_animator::set_watch_jump_queue(std::string const& jump_queue_name,
                                               bool watch,
                                               uint32_t priority)
{
    auto& jq{ m_jump_queue_name_to_jump_queue_map.at(jump_queue_name) };
    if (jq.is_watching != watch)
    {
        jq.is_watching = watch;
        jq.priority = priority;
        return true;
    }
    else
    {
        return false;
    }
}

std::optional<TXP::Model_animator::Animator_state_set> TXP::Model_animator::pop_one_state_set()
{
    std::optional<Animator_state_set> state_set{ std::nullopt };

    // Collect watching jump queues sorted by priority.
    std::map<uint32_t, std::vector<Jump_queue_data::State_set_queue_item>*>
        sorted_priority_to_state_set_queue;
    for (auto& [_, jq] : m_jump_queue_name_to_jump_queue_map)
    {
        if (jq.is_watching)
        {
            bool success =
                sorted_priority_to_state_set_queue.emplace(jq.priority, &jq.state_set_queue).second;
            if (!success)
            {
                BT_ERRORF("Doubled-up priority level: %u", jq.priority);
                assert(false);
            }
        }
    }

    // Fetch first non-expired state-set (first one is highest priority).
    for (auto& [_, jq] : sorted_priority_to_state_set_queue)
    {
        size_t i{ 0 };
        for (; i < jq->size(); i++)
        {
            if (s_sim_timer.load() < (*jq)[i].queue_expire_time_absolute)
            {
                state_set = (*jq)[i].state_set;
                i++;  // To ensure that this state-set gets deleted as well.
                break;
            }
        }

        size_t pre_delete_size{ jq->size() };

        // Remove expired queue items.
        for (size_t j = 0; j < i; j++)
            jq->erase(jq->begin());

        size_t post_delete_size{ jq->size() };
        if (pre_delete_size != post_delete_size)
        {
            BT_WARNF("Jump-queue %u : size %llu -> %llu", _, pre_delete_size, post_delete_size);
        }

        // Exit early if state set is found.
        if (state_set.has_value())
            break;
    }

    return state_set;
}

// Please ignore the const_cast's below!! (^_^;)

TXP::Model_animator::animator_time_t& TXP::Model_animator::get_profile_time_handle(
    Animator_timer_profile profile) const
{
    switch (profile)
    {
    case SIMULATION_PROFILE: return const_cast<animator_time_t&>(m_sim_time);
    case RENDERER_PROFILE:   return const_cast<animator_time_t&>(m_rend_time);

    default:
        assert(false);
        return *reinterpret_cast<animator_time_t*>(0xDEADBEEF);
        break;
    }
}

TXP::Model_animator::animator_frame_t& TXP::Model_animator::get_profile_prev_frame_handle(
    Animator_timer_profile profile) const
{
    switch (profile)
    {
    case SIMULATION_PROFILE: return const_cast<animator_frame_t&>(m_sim_prev_frame);

    default:
        assert(false);
        return *reinterpret_cast<animator_frame_t*>(0xDEADBEEF);
        break;
    }
}

void TXP::Model_animator::execute_command_code(cmd_code_t const& cmd_code,
                                               uint32_t row_idx,
                                               bool is_reg_first_frame,
                                               bool is_reg_last_frame)
{
    auto const& cmd_docs{ get_control_command_codes_documentation() };
    bool executed{ false };
    for (auto const& cmd_doc : cmd_docs)
    {
        if (cmd_code.cmd_name == cmd_doc.cmd.name)
        {   // Execute this cmd.
            cmd_doc.exec_fn(*this, row_idx, is_reg_first_frame, is_reg_last_frame, cmd_code.argv);
            executed = true;
            break;
        }
    }

    if (!executed)
    {
        BT_ERRORF("Executing cmd code not found: \"%s\"", cmd_code.cmd_name.c_str());
        assert(false);
    }
}

bool TXP::Model_animator::change_state_set_state_idx_goto_next(bool reset_count)
{
    uint32_t from_state_copy{ m_current_state_set_state_idx.load() };
    uint32_t to_state{ reset_count ? 0 : from_state_copy + 1 };

    std::lock_guard<std::mutex> lock{ m_current_state_set.mutex };
    uint32_t num_states_in_state_set = m_current_state_set.state_set.anim_state_indices.size();
    assert(num_states_in_state_set >= 1);

    if (!reset_count && to_state >= num_states_in_state_set)
    {   // No transition.
        // @NOTE: no transition for last transition since animator will detect whether to loop or
        //   pause at end, with this function having no responsibility in that.
        //     -Thea 2026/01/23
        return false;
    }

    // Do state idx transition.
    if (!(to_state < num_states_in_state_set))
    {
        BT_ERRORF("Invalid state-set state idx: %u (when there are %u in the state-set)",
                  to_state,
                  num_states_in_state_set);
        assert(false);
        return false;
    }
    if (m_current_state_set_state_idx
            .compare_exchange_strong(from_state_copy,
                                     to_state))
    {   // Reset all time profiles.
        reset_time();
    }

    // Successful transition!
    return true;
}

TXP::Model_animator::Pair_state_set_info TXP::Model_animator::
    get_animator_state_info_from_current_state_set() const
{
    std::lock_guard<std::mutex> lock{ m_current_state_set.mutex };

    auto const& sss_indices{ m_current_state_set.state_set.anim_state_indices };

    // In case of bad/unset state-set, return invalid info.
    if (sss_indices.empty())
    {
        BT_WARN("Empty state-set detected!");
        return { .animator_state_idx = (uint32_t)-1,
                 .loop = false };
    }

    auto cur_sss_idx{ m_current_state_set_state_idx.load() };
    auto is_final_state{ cur_sss_idx == sss_indices.size() - 1 };

    return { .animator_state_idx = sss_indices[cur_sss_idx],
             .loop = is_final_state && m_current_state_set.state_set.loop_final_state };
}

TXP::anim_tmpl_types::Animator_variable& TXP::Model_animator::find_animator_variable(
    std::string const& var_name)
{
    return const_cast<anim_tmpl_types::Animator_variable&>(find_animator_variable_const(var_name));
}

TXP::anim_tmpl_types::Animator_variable const& TXP::Model_animator::find_animator_variable_const(
    std::string const& var_name) const
{
    for (auto& anim_var : m_animator_variables)
        if (anim_var.var_name == var_name)
        {   // Found variable!
            return anim_var;
        }

    // Crash the program when don't find the var.
    assert(false);
    throw std::runtime_error(("Did not find var name: " + var_name).c_str());
}

TXP::Model_animator::Blend_value_result TXP::Model_animator::calc_blend_value_of_blendtree(
    anim_tmpl_types::Animator_state const& anim_state) const
{   // Look for two animations that are closest.
    float_t blend_var_value{ get_float_variable(anim_state.blend_var) };

    std::map<float_t, uint32_t> distance_to_anim_idx_map;
    for (auto const& blend_anim : anim_state.blend_anims)
    {
        distance_to_anim_idx_map.emplace(std::abs(blend_anim.value - blend_var_value),
                                            blend_anim.animation_idx);
    }
    assert(distance_to_anim_idx_map.size() >= 2);

    std::vector<std::pair<float_t, uint32_t>> influencing_distance_and_anim_idx_pairs;
    influencing_distance_and_anim_idx_pairs.reserve(2);
    for (auto it : distance_to_anim_idx_map)
    {
        influencing_distance_and_anim_idx_pairs.emplace_back(it.first, it.second);
        if (influencing_distance_and_anim_idx_pairs.size() >= 2)
            break;
    }

    // Blend motions.
    float_t blend_t;
    {
        float_t total_weight{ influencing_distance_and_anim_idx_pairs[0].first +
                                influencing_distance_and_anim_idx_pairs[1].first };
        blend_t = (influencing_distance_and_anim_idx_pairs[0].first / total_weight);
    }

    return { influencing_distance_and_anim_idx_pairs[0].second,
             influencing_distance_and_anim_idx_pairs[1].second,
             blend_t };
}
