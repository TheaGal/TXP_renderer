#include "hitcapsule.h"

#include "btservice_finder.h"
#include "btglm.h"
#include "btlogger.h"
#include "btuuid.h"
// #include "../renderer/debug_render_job.h"
#include "render_object/skeletal_animator.h"

#include <cassert>


// Hitcapsule.
void TXP::Hitcapsule::init_calc_info(Model_animator const& animator)
{
    auto& joint_name_to_idx{ animator.get_model_skin().joint_name_to_idx };

    if (!connecting_bone_name.empty())
        calcd_bone_mat_idx = joint_name_to_idx.at(connecting_bone_name);
    if (!connecting_bone_name_2.empty())
        calcd_bone_mat_idx_2 = joint_name_to_idx.at(connecting_bone_name_2);

    glm_vec3_copy(origin_a.raw, calcd_origin_a);
    glm_vec3_copy(origin_b.raw, calcd_origin_b);

    calc_orig_pt_distance();
}

void TXP::Hitcapsule::update_transform(mat4 base_transform, std::vector<mat4s> const& joint_matrices)
{
    static auto s_eval_combined_transforms_fn = [](size_t connecting_bone_mat_idx,
                                                   size_t connecting_bone_mat_idx_2,
                                                   mat4 base_transform,
                                                   std::vector<mat4s> const& joint_matrices,
                                                   mat4& out_combined_trans,
                                                   mat4& out_combined_trans_2) {
        if (connecting_bone_mat_idx == (size_t)-1)
        {
            glm_mat4_copy(base_transform, out_combined_trans);
            glm_mat4_copy(out_combined_trans, out_combined_trans_2);
        }
        else
        {
            glm_mat4_mul(base_transform,
                         const_cast<vec4*>(joint_matrices[connecting_bone_mat_idx].raw),
                         out_combined_trans);

            // Second connecting bone.
            if (connecting_bone_mat_idx_2 == (size_t)-1)
            {
                glm_mat4_copy(out_combined_trans, out_combined_trans_2);
            }
            else
            {
                glm_mat4_mul(base_transform,
                             const_cast<vec4*>(joint_matrices[connecting_bone_mat_idx_2].raw),
                             out_combined_trans_2);
            }
        }
    };

    // Update capsule origins with combined transform(s).
    mat4 combined_trans;
    mat4 combined_trans_2;
    s_eval_combined_transforms_fn(calcd_bone_mat_idx,
                                  calcd_bone_mat_idx_2,
                                  base_transform,
                                  joint_matrices,
                                  combined_trans,
                                  combined_trans_2);

    glm_mat4_mulv3(combined_trans, origin_a.raw, 1.0f, calcd_origin_a);
    glm_mat4_mulv3(combined_trans_2, origin_b.raw, 1.0f, calcd_origin_b);

    // Recalc origin point dependent vars.
    calc_orig_pt_distance();
}

void TXP::Hitcapsule::calc_orig_pt_distance()
{
    calcd_orig_pts_dist = glm_vec3_distance(calcd_origin_a, calcd_origin_b);
}

void TXP::Hitcapsule::emplace_debug_render_repr(vec4 color) const
{
    assert(false);  // @TODO
//     get_main_debug_line_pool().emplace_debug_line_based_capsule(
//         const_cast<float_t*>(calcd_origin_a),
//         const_cast<float_t*>(calcd_origin_b),
//         radius,
//         color,
// #define BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD 0
// #if BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD
//         0.5f);
// #else
//         0.03f);
// #endif
}


// Hitcapsule_group.
void TXP::Hitcapsule_group::set_enabled(bool enabled)
{
    m_enabled = enabled;
}

bool TXP::Hitcapsule_group::is_enabled() const
{
    return m_enabled;
}

TXP::Hitcapsule_group::Type TXP::Hitcapsule_group::get_type() const
{
    return m_type;
}

std::vector<TXP::Hitcapsule>& TXP::Hitcapsule_group::get_capsules()
{
    return m_capsules;
}

void TXP::Hitcapsule_group::emplace_debug_render_repr() const
{   // Generate color.
    vec4s color;
    switch (m_type)
    {
        case HITBOX_TYPE_RECEIVE_HURT:
            color = (m_enabled
                     ? vec4s{ 0.509, 0.465, 0.990 }
                     : vec4s{ 0.201, 0.183, 0.390 });
            break;

        case HITBOX_TYPE_GIVE_HURT:
            color = (m_enabled
                     ? vec4s{ 0.950, 0.427, 0.00 }
                     : vec4s{ 0.530, 0.288, 0.0901 });
            break;
    }

    // "Draw" hitcapsules.
    for (auto& hitcapsule : m_capsules)
    {
        hitcapsule.emplace_debug_render_repr(color.raw);
    }
}


// Hitcapsule_group_set.
TXP::Hitcapsule_group_set::~Hitcapsule_group_set()
{
    unregister_from_overlap_solver();
}

void TXP::Hitcapsule_group_set::replace_and_reregister(Hitcapsule_group_set const& other,
                                                       BT::UUID response_uuid)
{
    auto& overlap_solver{ BT::service_finder::find_service<Hitcapsule_group_overlap_solver>() };

    if (m_is_registered_in_overlap_solver)
    {   // Unregister self if currently registered.  @COPYPASTA with `unregister_from_overlap_solver()`
        m_is_registered_in_overlap_solver =
            !overlap_solver.remove_group_set(*this);
        assert(!m_is_registered_in_overlap_solver);
    }

    *this = other;

    m_is_registered_in_overlap_solver = overlap_solver.add_group_set(*this);
    assert(m_is_registered_in_overlap_solver);

    m_resp_entity_uuid = response_uuid;
    assert(!m_resp_entity_uuid.is_nil());
}

void TXP::Hitcapsule_group_set::unregister_from_overlap_solver()
{
    if (m_is_registered_in_overlap_solver)
    {
        // Unregister to cleanup.
        m_is_registered_in_overlap_solver =
            !BT::service_finder::find_service<Hitcapsule_group_overlap_solver>().remove_group_set(*this);
    }
}

void TXP::Hitcapsule_group_set::connect_animator(Model_animator const& animator)
{
    for (auto& group : m_hitcapsule_grps)
    for (auto& capsule : group.get_capsules())
    {
        capsule.init_calc_info(animator);
    }

    m_is_connected_to_animator = true;
}

std::vector<TXP::Hitcapsule_group>& TXP::Hitcapsule_group_set::get_hitcapsule_groups()
{
    return m_hitcapsule_grps;
}

BT::UUID TXP::Hitcapsule_group_set::get_resp_entity_uuid() const
{
    return m_resp_entity_uuid;
}

void TXP::Hitcapsule_group_set::emplace_debug_render_repr() const
{
    for (auto& group : m_hitcapsule_grps)
    {
        group.emplace_debug_render_repr();
    }
}

std::vector<TXP::Hitcapsule const*> TXP::Hitcapsule_group_set::fetch_all_enabled_hitcapsules(
    Hitcapsule_group::Type hitcapsule_type) const
{
    std::vector<Hitcapsule const*> result_hitcapsules;

    for (auto& group : m_hitcapsule_grps)
        if (group.is_enabled() && group.get_type() == hitcapsule_type)
        {
            auto const& hitcapsules{ const_cast<Hitcapsule_group&>(group).get_capsules() };  // @HACK.

            for (auto& hitcapsule : hitcapsules)
                result_hitcapsules.emplace_back(&hitcapsule);
        }

    result_hitcapsules.shrink_to_fit();
    return result_hitcapsules;    
}


// Hitcapsule_group_overlap_solver.
TXP::Hitcapsule_group_overlap_solver::Hitcapsule_group_overlap_solver()
{
    // Add self as service.
    BT_SERVICE_FINDER_ADD_SERVICE(Hitcapsule_group_overlap_solver, this);
}

namespace
{
constexpr char const* const k_add_log_str =
    "(+) Added hitcapsule group set to overlap solver:     ";
constexpr char const* const k_remove_log_str =
    "(-) Removed hitcapsule group set from overlap solver: ";
}  // namespace

bool TXP::Hitcapsule_group_overlap_solver::add_group_set(Hitcapsule_group_set& group_set)
{
    bool success{ false };
    if (m_group_sets.emplace(&group_set).second)
    {
        BT_TRACEF("%s%p", k_add_log_str, (void*)&group_set);
        success = true;
    }
    else
        BT_ERROR("Emplacing hitcapsule group set failed.");

    assert(success);
    return success;
}

bool TXP::Hitcapsule_group_overlap_solver::remove_group_set(Hitcapsule_group_set& group_set)
{
    bool success{ false };
    auto iter{ m_group_sets.find(&group_set) };
    if (iter != m_group_sets.end())
    {
        m_group_sets.erase(iter);
        BT_TRACEF("%s%p", k_remove_log_str, (void*)&group_set);
        success = true;
    }

    return success;
}

size_t TXP::Hitcapsule_group_overlap_solver::get_num_group_sets() const
{
    return m_group_sets.size();
}

TXP::Overlap_result_set TXP::Hitcapsule_group_overlap_solver::update_overlaps()
{
    Overlap_result_set result;

    // Check for all capsule group overlaps.
    for (auto grp_set_ptr_a : m_group_sets)
    for (auto grp_set_ptr_b : m_group_sets)
    if (grp_set_ptr_a != grp_set_ptr_b)
    {   // Collect capsules for check.
        auto give_hurt_caps_a{ grp_set_ptr_a->fetch_all_enabled_hitcapsules(
            Hitcapsule_group::HITBOX_TYPE_GIVE_HURT) };
        auto rece_hurt_caps_b{ grp_set_ptr_b->fetch_all_enabled_hitcapsules(
            Hitcapsule_group::HITBOX_TYPE_RECEIVE_HURT) };

        // Overlap check (A hurts B).
        // Does broad phase first, then if passes, does narrow phase.
        // @NOTE: Only A hurts B is necessary, since the n^2 loops. The reverse case will eventually
        //        be covered with the loops.
        bool found_hurt_overlap{ false };
        {
            for (size_t a_i = 0; a_i < give_hurt_caps_a.size() && !found_hurt_overlap; a_i++)
            for (size_t b_i = 0; b_i < rece_hurt_caps_b.size() && !found_hurt_overlap; b_i++)
            {
                if (check_broad_phase_hitcapsule_pair(*give_hurt_caps_a[a_i], *rece_hurt_caps_b[b_i]) &&
                    check_narrow_phase_hitcapsule_pair(*give_hurt_caps_a[a_i], *rece_hurt_caps_b[b_i]))
                {   // Found overlap! No more need to look.
                    found_hurt_overlap = true;
                    break;
                }
            }
        }

        if (found_hurt_overlap)
        {   // Report that A hurt B!
            result.give_rece_hurt_pairs.emplace_back(grp_set_ptr_a->get_resp_entity_uuid(),
                                                     grp_set_ptr_b->get_resp_entity_uuid());
        }
    }

    // Submit debug drawing for all hitcapsules.
    for (auto group_set_ptr : m_group_sets)
    {
        group_set_ptr->emplace_debug_render_repr();
    }

    return result;
}

bool TXP::Hitcapsule_group_overlap_solver::check_broad_phase_hitcapsule_pair(
    Hitcapsule const& give_hurt_capsule,
    Hitcapsule const& receive_hurt_capsule) const
{   // Sphere-to-sphere overlap.
    auto cap_a_origin_a{ const_cast<float_t*>(give_hurt_capsule.calcd_origin_a) };
    auto cap_a_origin_b{ const_cast<float_t*>(give_hurt_capsule.calcd_origin_b) };
    auto cap_a_orig_pts_dist{ give_hurt_capsule.calcd_orig_pts_dist };
    auto cap_a_radius{ give_hurt_capsule.radius };

    auto cap_b_origin_a{ const_cast<float_t*>(receive_hurt_capsule.calcd_origin_a) };
    auto cap_b_origin_b{ const_cast<float_t*>(receive_hurt_capsule.calcd_origin_b) };
    auto cap_b_orig_pts_dist{ give_hurt_capsule.calcd_orig_pts_dist };
    auto cap_b_radius{ receive_hurt_capsule.radius };

    // Calculate bounding sphere over capsules.
    vec3 a_mdpt_origin = GLM_VEC3_ZERO_INIT;
    glm_vec3_muladds(cap_a_origin_a, 0.5f, a_mdpt_origin);
    glm_vec3_muladds(cap_a_origin_b, 0.5f, a_mdpt_origin);
    float_t a_bp_sphere_rad{ cap_a_orig_pts_dist * 0.5f + cap_a_radius };

    vec3 b_mdpt_origin = GLM_VEC3_ZERO_INIT;
    glm_vec3_muladds(cap_b_origin_a, 0.5f, b_mdpt_origin);
    glm_vec3_muladds(cap_b_origin_b, 0.5f, b_mdpt_origin);
    float_t b_bp_sphere_rad{ cap_b_orig_pts_dist * 0.5f + cap_b_radius };

    // Check for overlap.
    bool found_overlap{ false };

    float_t combined_rads{ a_bp_sphere_rad + b_bp_sphere_rad };
    if (glm_vec3_distance2(a_mdpt_origin, b_mdpt_origin) < combined_rads * combined_rads)
    {   // Found overlap!
        found_overlap = true;

        #if 0
        // BT_TRACEF("Broad phase passed.\n"
        //           "  A orig A: (%.3f, %.3f, %.3f)\n"
        //           "  A orig B: (%.3f, %.3f, %.3f)\n"
        //           "  A c_dist:  %.3f\n"
        //           "  A radius: %.3f\n"
        //           "  B orig A: (%.3f, %.3f, %.3f)\n"
        //           "  B orig B: (%.3f, %.3f, %.3f)\n"
        //           "  B c_dist:  %.3f\n"
        //           "  B radius: %.3f\n",
        //           cap_a_origin_a[0], cap_a_origin_a[1], cap_a_origin_a[2],
        //           cap_a_origin_b[0], cap_a_origin_b[1], cap_a_origin_b[2],
        //           cap_a_orig_pts_dist,
        //           cap_a_radius,
        //           cap_b_origin_a[0], cap_b_origin_a[1], cap_b_origin_a[2],
        //           cap_b_origin_b[0], cap_b_origin_b[1], cap_b_origin_b[2],
        //           cap_b_orig_pts_dist,
        //           cap_b_radius);

        get_main_debug_line_pool().emplace_debug_line_based_capsule(
            a_mdpt_origin,
            a_mdpt_origin,
            a_bp_sphere_rad,
            vec4{ 0.0285, 0.570, 0.00 },
        #if BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD
            0.5f);
        #else
            0.03f);
        #endif

        get_main_debug_line_pool().emplace_debug_line_based_capsule(
            b_mdpt_origin,
            b_mdpt_origin,
            b_bp_sphere_rad,
            vec4{ 0.478, 0.590, 0.0295 },
        #if BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD
            0.5f);
        #else
            0.03f);
        #endif

        #endif  // 0
    }

    return found_overlap;
}

bool TXP::Hitcapsule_group_overlap_solver::check_narrow_phase_hitcapsule_pair(
    Hitcapsule const& give_hurt_capsule,
    Hitcapsule const& receive_hurt_capsule) const
{   // Capsule-to-capsule overlap.

    // Ref: https://stackoverflow.com/questions/2824478/shortest-distance-between-two-line-segments (Fnord's answer)

    // Preparation.
    vec3 a0;
    glm_vec3_copy(const_cast<float_t*>(give_hurt_capsule.calcd_origin_a), a0);
    vec3 a1;
    glm_vec3_copy(const_cast<float_t*>(give_hurt_capsule.calcd_origin_b), a1);

    vec3 b0;
    glm_vec3_copy(const_cast<float_t*>(receive_hurt_capsule.calcd_origin_a), b0);
    vec3 b1;
    glm_vec3_copy(const_cast<float_t*>(receive_hurt_capsule.calcd_origin_b), b1);

    // Calculate denominator.
    vec3 A;
    glm_vec3_sub(a1, a0, A);
    vec3 B;
    glm_vec3_sub(b1, b0, B);

    auto magA{ glm_vec3_norm(A) };
    auto magB{ glm_vec3_norm(B) };

    vec3 _A;
    glm_vec3_scale(A, 1.0f / magA, _A);
    vec3 _B;
    glm_vec3_scale(B, 1.0f / magB, _B);

    vec3 cross;
    glm_vec3_cross(_A, _B, cross);
    auto denom{ glm_vec3_norm2(cross) };

    // Results.
    vec3 best_a;
    vec3 best_b;

    // If lines are parallel (denom=0), then test if lines overlap.
    if (glm_eq(denom, 0))
    {
        vec3 b0_a0;
        glm_vec3_sub(b0, a0, b0_a0);

        auto d0{ glm_vec3_dot(_A, b0_a0) };

        vec3 b1_a0;
        glm_vec3_sub(b1, a0, b1_a0);

        auto d1{ glm_vec3_dot(_A, b1_a0) };

        // Is segment B before A?
        if ((d0 < 0 || glm_eq(d0, 0)) && (d1 < 0 || glm_eq(d1, 0)))
        {
            if (std::abs(d0) < std::abs(d1))
            {
                glm_vec3_copy(a0, best_a);
                glm_vec3_copy(b0, best_b);
            }
            else
            {
                glm_vec3_copy(a0, best_a);
                glm_vec3_copy(b1, best_b);
            }
        }
        // Is segment B after A?
        else if ((d0 > magA || glm_eq(d0, magA)) && (d1 > 0 || glm_eq(d1, magA)))
        {
            if (std::abs(d0) < std::abs(d1))
            {
                glm_vec3_copy(a1, best_a);
                glm_vec3_copy(b0, best_b);
            }
            else
            {
                glm_vec3_copy(a1, best_a);
                glm_vec3_copy(b1, best_b);
            }
        }
        // Segments overlap.
        else
        {   // Just do a random spot on the line segments.
            glm_vec3_copy(a0, best_a);
            glm_vec3_copy(b0, best_b);
        }
    }
    // Lines cross-cross: Calculate the projected closest points.
    else
    {
        vec3 t;
        glm_vec3_sub(b0, a0, t);

        float_t detA;
        {
            mat3 m;
            glm_vec3_copy(t, m[0]);
            glm_vec3_copy(_B, m[1]);
            glm_vec3_copy(cross, m[2]);
            detA = glm_mat3_det(m);
        }
        float_t detB;
        {
            mat3 m;
            glm_vec3_copy(t, m[0]);
            glm_vec3_copy(_A, m[1]);
            glm_vec3_copy(cross, m[2]);
            detB = glm_mat3_det(m);
        }

        auto t0{ detA / denom };
        auto t1{ detB / denom };

        vec3 pA;  // Projected closest point on segment A.
        glm_vec3_copy(a0, pA);
        glm_vec3_muladds(_A, t0, pA);
        vec3 pB;  // Projected closest point on segment B.
        glm_vec3_copy(b0, pB);
        glm_vec3_muladds(_B, t1, pB);

        // Clamp projections.
        if (t0 < 0)
            glm_vec3_copy(a0, pA);
        else if (t0 > magA)
            glm_vec3_copy(a1, pA);

        if (t1 < 0)
            glm_vec3_copy(b0, pB);
        else if (t1 > magB)
            glm_vec3_copy(b1, pB);

        // Clamp projection A.
        if (t0 < 0 || t0 > magA)
        {
            vec3 pA_b0;
            glm_vec3_sub(pA, b0, pA_b0);

            auto dot{ glm_vec3_dot(_B, pA_b0) };
            dot = glm_clamp(dot, 0, magB);

            glm_vec3_copy(b0, pB);
            glm_vec3_muladds(_B, dot, pB);
        }

        // Clamp projection B.
        if (t1 < 0 || t1 > magB)
        {
            vec3 pB_a0;
            glm_vec3_sub(pB, a0, pB_a0);

            auto dot{ glm_vec3_dot(_A, pB_a0) };
            dot = glm_clamp(dot, 0, magA);

            glm_vec3_copy(a0, pA);
            glm_vec3_muladds(_A, dot, pA);
        }

        // Write result.
        glm_vec3_copy(pA, best_a);
        glm_vec3_copy(pB, best_b);
    }

    // Check for overlap.
    bool found_overlap{ false };

    auto cap_a_radius{ give_hurt_capsule.radius };
    auto cap_b_radius{ receive_hurt_capsule.radius };
    float_t combined_rads{ cap_a_radius + cap_b_radius };

    if (glm_vec3_distance2(best_a, best_b) < combined_rads * combined_rads)
    {   // Found overlap!
        found_overlap = true;
    }

    assert(false);  // @TODO: uncomment the below!!! (@THEA @NOCHECKIN)
    // if (found_overlap)
    // {   // @DEBUG show collision spheres.
    //     get_main_debug_line_pool().emplace_debug_line_based_capsule(
    //         best_a,
    //         best_a,
    //         cap_a_radius,
    //         vec4{ 0.610, 0.00, 0.254 },
    //     #if BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD
    //         0.5f);
    //     #else
    //         0.03f);
    //     #endif

    //     get_main_debug_line_pool().emplace_debug_line_based_capsule(
    //         best_b,
    //         best_b,
    //         cap_b_radius,
    //         vec4{ 0.560, 0.00560, 0.514 },
    //     #if BT_HITCAPSULE_DEBUG_RENDER_REPRESENTATION_LONG_HOLD
    //         0.5f);
    //     #else
    //         0.03f);
    //     #endif
    // }

    return found_overlap;
}
