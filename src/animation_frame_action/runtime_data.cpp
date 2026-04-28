// clang-format off
#include "txp_renderer/animation_frame_action/runtime_data.h"  // @TODO: split this cpp file into 2 files for each of these .h files
#include "runtime_data_controls.h"
// clang-format on

#include "render_object/animator_template.h"
// #include "../renderer/mesh.h"
#include "render_object/skeletal_animator.h"
#include "txp_renderer/animation_driven_hitcapsule/hitcapsule.h"

#include "btjson.h"
#include "btlogger.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>


// Controllable data.
void TXP::anim_frame_action::Runtime_controllable_data::Rising_edge_event::mark_rising_edge()
{
    m_rising_edge_count++;
}

bool TXP::anim_frame_action::Runtime_controllable_data::Rising_edge_event::
    check_if_rising_edge_occurred()
{
    if (m_rising_edge_count > 0)
    {
        m_rising_edge_count--;
        m__dev_re_ocurred_cooldown = 1.0f;
        return true;
    }
    else
        return false;
}

float_t TXP::anim_frame_action::Runtime_controllable_data::Rising_edge_event::
    update_cooldown_and_fetch_val(float_t delta_time)
{
    auto cooldown_prev_copy{ m__dev_re_ocurred_cooldown };
    m__dev_re_ocurred_cooldown = glm_max(0.0f,
                                         m__dev_re_ocurred_cooldown
                                         - delta_time);
    return cooldown_prev_copy;
}

TXP::anim_frame_action::Runtime_controllable_data::Controllable_data_type TXP::anim_frame_action::
    Runtime_controllable_data::get_data_type(Controllable_data_label label)
{
    Controllable_data_type ctrl_data_type;
    if (label > INTERNAL__CTRL_DATA_LABEL_MARKER_BEGIN_FLOAT &&
        label < INTERNAL__CTRL_DATA_LABEL_MARKER_END_FLOAT_BEGIN_BOOL)
    {
        ctrl_data_type = CTRL_DATA_TYPE_FLOAT;
    }
    else if (label > INTERNAL__CTRL_DATA_LABEL_MARKER_END_FLOAT_BEGIN_BOOL &&
             label < INTERNAL__CTRL_DATA_LABEL_MARKER_END_BOOL_BEGIN_REEVE)
    {
        ctrl_data_type = CTRL_DATA_TYPE_BOOL;
    }
    else if (label > INTERNAL__CTRL_DATA_LABEL_MARKER_END_BOOL_BEGIN_REEVE &&
             label < INTERNAL__CTRL_DATA_LABEL_MARKER_END_REEVE)
    {
        ctrl_data_type = CTRL_DATA_TYPE_RISING_EDGE_EVENT;
    }
    else
    {   // Unknown data type.
        assert(false);
        ctrl_data_type = CTRL_DATA_TYPE_UNKNOWN;
    }
    return ctrl_data_type;
}

TXP::anim_frame_action::Runtime_controllable_data::Overridable_data<float_t>& TXP::
    anim_frame_action::Runtime_controllable_data ::get_float_data_handle(
        Controllable_data_label label)
{
    assert(get_data_type(label) == CTRL_DATA_TYPE_FLOAT);
    return data_floats.at(label);
}

TXP::anim_frame_action::Runtime_controllable_data::Overridable_data<bool>& TXP::anim_frame_action::
    Runtime_controllable_data ::get_bool_data_handle(Controllable_data_label label)
{
    assert(get_data_type(label) == CTRL_DATA_TYPE_BOOL);
    return data_bools.at(label);
}

TXP::anim_frame_action::Runtime_controllable_data::Rising_edge_event& TXP::anim_frame_action::
    Runtime_controllable_data ::get_reeve_data_handle(Controllable_data_label label)
{
    assert(get_data_type(label) == CTRL_DATA_TYPE_RISING_EDGE_EVENT);
    return data_reeves.at(label);
}

void TXP::anim_frame_action::Runtime_controllable_data::clear_all_data_overrides()
{   // Populate the overridable data labels.
    static auto s_get_overridable_data_labels_fn = []() {
        std::vector<Controllable_data_label> overridable_data_labels;

        auto const& str_labels{ get_all_str_labels() };
        for (auto const& str_label : str_labels)
        {
            auto data_label{ str_label_to_enum(str_label) };
            switch (get_data_type(data_label))
            {
                case CTRL_DATA_TYPE_FLOAT:
                case CTRL_DATA_TYPE_BOOL:
                    overridable_data_labels.emplace_back(data_label);
                    break;

                default:
                    // Ignore since not overridable label.
                    break;
            }
        }

        return overridable_data_labels;
    };

    static auto s_overridable_data_labels{ s_get_overridable_data_labels_fn() };

    // Clear the data labels.
    for (auto data_label : s_overridable_data_labels)
    {
        switch (get_data_type(data_label))
        {
            case CTRL_DATA_TYPE_FLOAT:
                get_float_data_handle(data_label)
                    .clear_overriding();
                break;

            case CTRL_DATA_TYPE_BOOL:
                get_bool_data_handle(data_label)
                    .clear_overriding();
                break;

            default:
                // @NOTE: Should not enter this branch bc of prior filtering.
                assert(false);
                break;
        }
    }
}

void TXP::anim_frame_action::Runtime_controllable_data::map_animator_to_control_regions(
    component_internal::Model_animator const& animator,
    Runtime_data_controls const& data_controls)
{
    anim_state_idx_to_timeline_idx_map.clear();

    auto const& animator_states{ animator.get_animator_states() };
    auto const& data_control_timelines{ data_controls.data.anim_frame_action_timelines };

    if (animator_states.size() != data_control_timelines.size())
    {
        BT_ERROR(
            ".btanitor and .btafa num states check failed. Ensure that there are the same "
            "number of .btanitor anim states as there are .btafa timelines. Aborting program.");
        abort();
    }

    anim_state_idx_to_timeline_idx_map.reserve(animator_states.size());
    for (size_t anim_state_idx = 0; anim_state_idx < animator_states.size(); anim_state_idx++)
        for (size_t timeline_idx = 0; timeline_idx < data_control_timelines.size(); timeline_idx++)
            if (animator_states[anim_state_idx].state_name ==
                data_control_timelines[timeline_idx].state_name)
            {   // Found a mapping!
                anim_state_idx_to_timeline_idx_map.emplace(anim_state_idx, timeline_idx);
                break;
            }

    if (anim_state_idx_to_timeline_idx_map.size() != animator_states.size())
    {
        BT_ERROR(
            ">=1 anim states in .btanitor could not find a mapping into the .btafa. Check that the "
            ".btanitor and .btafa \"state_name\" props all match, bc >=1 aren\'t. Aborting "
            "program.");
        abort();
    }
}

void TXP::anim_frame_action::Runtime_controllable_data::assign_hitcapsule_enabled_flags()
{
    static std::vector<Controllable_data_label> const s_all_hitcapsule_grp_data_labels{
        CTRL_DATA_LABEL_hitcapsule_group_0_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_1_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_2_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_3_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_4_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_5_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_6_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_7_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_8_enabled,
        CTRL_DATA_LABEL_hitcapsule_group_9_enabled,
    };

    size_t data_label_idx{ 0 };

    auto& hitcapsule_grps{ hitcapsule_group_set.get_hitcapsule_groups() };
    for (auto& hitcapsule_grp : hitcapsule_grps)
    {
        assert(data_label_idx < s_all_hitcapsule_grp_data_labels.size());

        // Use data handle to set hitcapsule group enabled flag.
        hitcapsule_grp.set_enabled(
            get_bool_data_handle(s_all_hitcapsule_grp_data_labels[data_label_idx]).get_val());

        data_label_idx++;
    }
}

void TXP::anim_frame_action::Runtime_controllable_data::update_hitcapsule_transforms(
    mat4 base_transform,
    std::vector<mat4s> const& joint_matrices)
{
    auto& hitcapsule_grps{ hitcapsule_group_set.get_hitcapsule_groups() };
    for (auto& hitcapsule_grp : hitcapsule_grps)
    {
        auto& hitcapsules{ hitcapsule_grp.get_capsules() };
        for (auto& hitcapsule : hitcapsules)
        {   // Update hitcapsule to follow attached joint matrices.
            hitcapsule.update_transform(base_transform, joint_matrices);
        }
    }
}


// Data controls.
TXP::anim_frame_action::Runtime_data_controls::Runtime_data_controls(std::string const& fname)
{
    // Deserialize json into data.
    data = Data(BT::json_load_from_disk(fname));

    // Check timeline regions are sorted.
    for (auto const& tmln : data.anim_frame_action_timelines)
    {
        uint32_t cur_row{ 0 };
        for (auto const& region : tmln.regions)
        {
            if (region.row_idx < cur_row)
            {
                BT_ERRORF("Timeline regions not sorted. Prev=%u Cur=%u State_name=%s",
                          cur_row,
                          region.row_idx,
                          tmln.state_name.c_str());
                assert(false);
            }
            else
            {
                // Is sorted correctly.
                cur_row = region.row_idx;
            }
        }
    }

#if 0  // @THEA: @NOCHECKIN: FIX THIS AND MAKE SURE THAT THE MODEL EXISTS FROM THIS!!!!
    // Load model from bank.
    animated_model = Model_bank::get_model(data.animated_model_name);
    assert(animated_model != nullptr);
#endif // 0

    // @TODO: @THEA: add some kind of string->bytecode "compilation" step right here (really only
    //               for optimization in the future of course).  -Thea 2025/01/10
}


// Bank of data controls.
void TXP::anim_frame_action::Bank::emplace(std::string const& name,
                                           Runtime_data_controls&& runtime_state)
{
    s_runtime_states.emplace(name, std::move(runtime_state));
}

void TXP::anim_frame_action::Bank::replace(std::string const& name,
                                           Runtime_data_controls&& runtime_state)
{
    s_runtime_states.at(name) = std::move(runtime_state);
}

bool TXP::anim_frame_action::Bank::has(std::string const& name)
{
    return (s_runtime_states.find(name) != s_runtime_states.end());
}

TXP::anim_frame_action::Runtime_data_controls const& TXP::anim_frame_action::Bank::get(
    std::string const& name)
{
    return s_runtime_states.at(name);
}

std::vector<std::string> TXP::anim_frame_action::Bank::get_all_names()
{
    std::vector<std::string> all_names;
    all_names.reserve(s_runtime_states.size());

    for (auto& [key, data] : s_runtime_states)
    {
        all_names.emplace_back(key);
    }

    return all_names;
}
