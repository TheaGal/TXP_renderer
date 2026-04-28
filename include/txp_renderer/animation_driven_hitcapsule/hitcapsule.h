#pragma once

#include "btglm.h"
#include "btjson.h"
#include "btuuid.h"

#include <string>
#include <vector>
#include <unordered_set>


namespace TXP
{

namespace component_internal
{
class Model_animator;
}  // namespace component_internal

/// Individual hit-capsule. Multiple grouped together form a `Hitcapsule_group`.
struct Hitcapsule
{
    vec3s   origin_a;  // Origins of both ends of the spheres making up the capsule.
    vec3s   origin_b;
    float_t radius;

    std::string connecting_bone_name{ "" };  // Leave empty str for no bone connection.
    std::string connecting_bone_name_2{ "" };  // Leave empty str for same as `connecting_bone_name`.

    /// Serialization/deserialization.
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Hitcapsule,
                                   origin_a,
                                   origin_b,
                                   radius,
                                   connecting_bone_name,
                                   connecting_bone_name_2);

    // Calculated info.
    vec3    calcd_origin_a;       // Set to `origin_a` on init. Updated to connect to joint mat in
                                  // `update_transform`.
    vec3    calcd_origin_b;       // Similar to `calcd_origin_a`.
    float_t calcd_orig_pts_dist;  // Calculated from `calcd_origin_a/b`.

    size_t  calcd_bone_mat_idx{ (size_t)-1 };
    size_t  calcd_bone_mat_idx_2{ (size_t)-1 };  // -1 means same as `calcd_bone_mat_idx`.

    void init_calc_info(component_internal::Model_animator const& animator);
    void update_transform(mat4 base_transform, std::vector<mat4s> const& joint_matrices);
    void calc_orig_pt_distance();

    // Submits debug render representation of this capsule, with provided `color`.
    void emplace_debug_render_repr(vec4 color) const;
};

/// Group of hitcapsules with annotations of interactions of hurt.
class Hitcapsule_group
{
public:
    enum Type : uint8_t
    {
        HITBOX_TYPE_RECEIVE_HURT,
        HITBOX_TYPE_GIVE_HURT,  // @TODO: This could be a stabbing, sweeping, normal, or grab attack. This needs some kind of specification in the future at some point!  -Thea 2025/12/28
    };

    void set_enabled(bool enabled);
    bool is_enabled() const;
    Type get_type() const;
    std::vector<Hitcapsule>& get_capsules();

    // Submits debug render representation of hitcapsules, with different colors depending on type
    // of group and whether this group is enabled.
    void emplace_debug_render_repr() const;

private:
    bool m_enabled{ false };
    Type m_type{ 0 };
    std::vector<Hitcapsule> m_capsules;

public:
    /// Serialization/deserialization.
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Hitcapsule_group, m_enabled, m_type, m_capsules);
};

/// Set of hitcapsule groups to be connected to an entity.
class Hitcapsule_group_set
{
public:
    ~Hitcapsule_group_set();

    void replace_and_reregister(Hitcapsule_group_set const& other, BT::UUID resp_entity_uuid);
    void unregister_from_overlap_solver();

    void connect_animator(component_internal::Model_animator const& animator);

    std::vector<Hitcapsule_group>& get_hitcapsule_groups();

    /// Gets responsible entity UUID.
    BT::UUID get_resp_entity_uuid() const;

    /// Submits debug render representation of hitcapsule groups.
    void emplace_debug_render_repr() const;

    /// Retrieves all enabled hitcapsules for overlap checking.
    std::vector<Hitcapsule const*> fetch_all_enabled_hitcapsules(
        Hitcapsule_group::Type hitcapsule_type) const;

private:
    std::vector<Hitcapsule_group> m_hitcapsule_grps;
    bool m_is_connected_to_animator{ false };
    bool m_is_registered_in_overlap_solver{ false };
    BT::UUID m_resp_entity_uuid;  // The entity UUID that is responsible for responding for overlaps.

public:
    /// Serialization/deserialization.
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Hitcapsule_group_set, m_hitcapsule_grps);
};

/// Result set for checking overlaps in the solver. With each `Entity_pair_list`, the first UUID is
/// the giver and the second UUID is the receiver.
struct Overlap_result_set
{
    using Entity_pair_list = std::vector<std::pair<BT::UUID, BT::UUID>>;
    Entity_pair_list give_rece_hurt_pairs;
};

/// Solver for groups of overlapping hitcapsule group sets (i.e. entity-to-entity hitcapsule
/// overlaps).
class Hitcapsule_group_overlap_solver
{
public:
    Hitcapsule_group_overlap_solver();

    bool add_group_set(Hitcapsule_group_set& group_set);
    bool remove_group_set(Hitcapsule_group_set& group_set);
    size_t get_num_group_sets() const;

    Overlap_result_set update_overlaps();

private:
    std::unordered_set<Hitcapsule_group_set const*> m_group_sets;

    bool check_broad_phase_hitcapsule_pair(Hitcapsule const& give_hurt_capsule,
                                           Hitcapsule const& receive_hurt_capsule) const;
    bool check_narrow_phase_hitcapsule_pair(Hitcapsule const& give_hurt_capsule,
                                            Hitcapsule const& receive_hurt_capsule) const;
};

}  // namespace TXP
