#pragma once

#include "animation_driven_hitcapsule/hitcapsule.h"  // for `Hitcapsule_group_set`
#include "btglm.h"
#include "btjson.h"

#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

namespace component_internal
{
class Model_animator;
}  // namespace component_internal

namespace anim_frame_action
{

// Controllable data.
enum Controllable_data_label : std::uint32_t
{
    #define TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST   \
        /* Floats */                                    \
        X_float(model_opacity,                 1.0f)    \
        X_float(turn_speed,                    0.0f)    \
        X_float(mvt_input_max_speed,           0.0f)    \
        X_float(mvt_input_accel,               0.0f)    \
        X_float(mvt_input_decel,               0.0f)    \
        X_float(root_motion_multi,             1.0f)    \
        X_float(attack_send_root_motion_multi, 1.0f)    \
        X_float(gravity_magnitude,             1.0f)    \
        /* Bools */                                     \
        X__bool(is_parry_active,            false)      \
        X__bool(is_guard_active,            false)      \
        X__bool(can_move,                   true)       \
        X__bool(can_do_turnaround_anim,     false)      \
        X__bool(mvt_input_enabled,          false)      \
        X__bool(can_guard_exit,             true)       \
        X__bool(can_attack_exit,            true)       \
        X__bool(can_cancel_attack_w_parry,  false)      \
        X__bool(blade_has_mizunokata,       false)      \
        X__bool(blade_has_honoonokata,      false)      \
        X__bool(hitcapsule_group_0_enabled, false)      \
        X__bool(hitcapsule_group_1_enabled, false)      \
        X__bool(hitcapsule_group_2_enabled, false)      \
        X__bool(hitcapsule_group_3_enabled, false)      \
        X__bool(hitcapsule_group_4_enabled, false)      \
        X__bool(hitcapsule_group_5_enabled, false)      \
        X__bool(hitcapsule_group_6_enabled, false)      \
        X__bool(hitcapsule_group_7_enabled, false)      \
        X__bool(hitcapsule_group_8_enabled, false)      \
        X__bool(hitcapsule_group_9_enabled, false)      \
        /* Rising edge events */                        \
        X_reeve(cpu_aware_chg_suspicious_to_unaware)    \
        X_reeve(broadcast_attack_to_enemies)            \
        X_reeve(broadcast_healing_to_enemies)           \
        X_reeve(play_sfx_footstep)                      \
        X_reeve(play_sfx_ready_guard)                   \
        X_reeve(play_sfx_blade_swing)                   \
        X_reeve(play_sfx_hurt_vocalize_human_male_mc)   \
        X_reeve(play_sfx_guard_receive_hit)             \
        X_reeve(play_sfx_deflect_receive_hit)

    INTERNAL__CTRL_DATA_LABEL_MARKER_BEGIN_FLOAT = 0,

    #define X_float(name, def_val)  CTRL_DATA_LABEL_##name,
    #define X__bool(name, def_val)
    #define X_reeve(name)
    TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
    #undef X_float
    #undef X__bool
    #undef X_reeve

    INTERNAL__CTRL_DATA_LABEL_MARKER_END_FLOAT_BEGIN_BOOL,

    #define X_float(name, def_val)
    #define X__bool(name, def_val)  CTRL_DATA_LABEL_##name,
    #define X_reeve(name)
    TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
    #undef X_float
    #undef X__bool
    #undef X_reeve

    INTERNAL__CTRL_DATA_LABEL_MARKER_END_BOOL_BEGIN_REEVE,

    #define X_float(name, def_val)
    #define X__bool(name, def_val)
    #define X_reeve(name)           CTRL_DATA_LABEL_##name,
    TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
    #undef X_float
    #undef X__bool
    #undef X_reeve

    INTERNAL__CTRL_DATA_LABEL_MARKER_END_REEVE
};

/// Forward decl.
struct Runtime_data_controls;

// Controllable data.
struct Runtime_controllable_data
{   // Get list of str labels.
    static std::vector<std::string> const& get_all_str_labels()
    {
        static std::vector<std::string> s_all_labels{
            #define X_float(name, def_val)  #name,
            #define X__bool(name, def_val)  #name,
            #define X_reeve(name)           #name,
            TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
            #undef X_float
            #undef X__bool
            #undef X_reeve
        };
        return s_all_labels;
    }

    // Lookup str->enum.
    static Controllable_data_label str_label_to_enum(std::string const& str_label)
    {
        static std::unordered_map<std::string, Controllable_data_label> s_label_to_enum_map{
            #define X_float(name, def_val)  { #name, CTRL_DATA_LABEL_##name },
            #define X__bool(name, def_val)  { #name, CTRL_DATA_LABEL_##name },
            #define X_reeve(name)           { #name, CTRL_DATA_LABEL_##name },
            TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
            #undef X_float
            #undef X__bool
            #undef X_reeve
        };
        return s_label_to_enum_map.at(str_label);
    }

    // Lookup data type.
    enum Controllable_data_type
    {
        CTRL_DATA_TYPE_FLOAT,
        CTRL_DATA_TYPE_BOOL,
        CTRL_DATA_TYPE_RISING_EDGE_EVENT,
        CTRL_DATA_TYPE_UNKNOWN,
    };
    static Controllable_data_type get_data_type(Controllable_data_label label);

    // Overridable data type.
    template<typename T>
    class Overridable_data
    {
    public:
        explicit Overridable_data(T val)
            : m_persistant_val{ val }
            , m_overriding_val{ val }
        {
        }

        void clear_overriding()
        {   // Clear upon every change in the timeline.
            m_use_overriding_val = false;
        }

        void write_val(T val)
        {
            m_persistant_val = val;
        }

        void override_val(T val)
        {
            m_overriding_val = val;
            m_use_overriding_val = true;
        }

        T get_val() const
        {
            return (m_use_overriding_val ? m_overriding_val : m_persistant_val);
        }
    private:
        T m_persistant_val;
        T m_overriding_val;
        bool m_use_overriding_val{ false };
    };

    // Rising edge event class.
    class Rising_edge_event
    {
    public:
        void mark_rising_edge();
        bool check_if_rising_edge_occurred();
        float_t update_cooldown_and_fetch_val(float_t delta_time);
    private:
        uint32_t m_rising_edge_count{ 0 };
        float_t m__dev_re_ocurred_cooldown{ 0.0f };
    };

private:
    // Lookup reading/writing handle for data.
    std::unordered_map<Controllable_data_label, Overridable_data<float_t>> data_floats{
        #define X_float(name, def_val)  { CTRL_DATA_LABEL_##name, Overridable_data<float_t>(def_val) },
        #define X__bool(name, def_val)
        #define X_reeve(name)
        TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
        #undef X_float
        #undef X__bool
        #undef X_reeve
    };
    std::unordered_map<Controllable_data_label, Overridable_data<bool>> data_bools{
        #define X_float(name, def_val)
        #define X__bool(name, def_val)  { CTRL_DATA_LABEL_##name, Overridable_data<bool>(def_val) },
        #define X_reeve(name)
        TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
        #undef X_float
        #undef X__bool
        #undef X_reeve
    };
    std::unordered_map<Controllable_data_label, Rising_edge_event> data_reeves{
        #define X_float(name, def_val)
        #define X__bool(name, def_val)
        #define X_reeve(name)           { CTRL_DATA_LABEL_##name, Rising_edge_event{} },
        TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST
        #undef X_float
        #undef X__bool
        #undef X_reeve
    };

    #undef TXP_MODEL_ANIMATOR_CONTROLLABLE_DATA_LIST

public:
    Overridable_data<float_t>& get_float_data_handle(Controllable_data_label label);
    Overridable_data<bool>& get_bool_data_handle(Controllable_data_label label);
    Rising_edge_event& get_reeve_data_handle(Controllable_data_label label);

    void clear_all_data_overrides();

    /// Map to get timeline idx for runtime controls.
    std::unordered_map<size_t, size_t> anim_state_idx_to_timeline_idx_map;

    /// Calculates the mapping of animator state indices to timeline indices.
    void map_animator_to_control_regions(component_internal::Model_animator const& animator,
                                         Runtime_data_controls const& data_controls);

    // Controlled hitcapsule group set.
    Hitcapsule_group_set hitcapsule_group_set;

    // Update all hitcapsule groups' enabled flags to match
    // the runtime controllable data enabled flags.
    void assign_hitcapsule_enabled_flags();

    // Updates hitcapsule transforms to `joint_matrices[x]` where `x` is the connecting bone matrix
    // index. Supply a game object transform in `base_transform`.
    void update_hitcapsule_transforms(mat4 base_transform,
                                      std::vector<mat4s> const& joint_matrices);

public:
    /// Serialization/deserialization.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Runtime_controllable_data,
                                       hitcapsule_group_set);
};

}  // namespace anim_frame_action
}  // namespace TXP
