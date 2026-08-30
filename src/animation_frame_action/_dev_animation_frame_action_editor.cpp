#include "_dev_animation_frame_action_editor.h"

#include "btservice_finder.h"
#include "btuuid.h"
#include "editor_state.h"
#include "entt/entity/registry.hpp"
#include "render_object/skeletal_animator.h"
#include "txp_renderer/animation_frame_action/_dev_animation_frame_action_editor_agent.h"
#include "txp_renderer/animation_frame_action/runtime_data.h"
#include "txp_renderer/animator/skeletal_animator.h"
#include "txp_renderer/renderer.h"
#include "txp_renderer/types.h"

#include <cassert>
#include <optional>


void TXP::system::_dev_animation_frame_action_editor(entt::registry& reg)
{
    auto& renderer{ BT::service_finder::find_service<Renderer>() };
    auto view{ reg.view<component::_Dev_animation_frame_action_editor_agent>() };
    auto& eds{ anim_frame_action::s_editor_state };

    // This check is to ensure that editing the editor state will be used/practical.
    assert(view->size() <= 1);

    // Reset all agents.
    bool reset_agent_model_flag{ false };
    if (eds.request_reset_agent_model)
    {
        eds.request_reset_agent_model = false;
        reset_agent_model_flag = true;
    }

    for (auto&& [entity, afa_agent] : view->each())
    {
        // Setup or reset agent model.
        if (reset_agent_model_flag || !reg.any_of<component::Render_object_config>(entity))
        {
            auto& rend_obj_cfg = reg.emplace_or_replace<component::Render_object_config>(entity);
            rend_obj_cfg.render_layer = RENDER_LAYER_DEFAULT;
            rend_obj_cfg.model_name = eds.working_model_name;
            rend_obj_cfg.is_deformed = true;

            eds.working_model_animator = nullptr;
        }
        // Configuration once render object is created.
        else if (auto try_animator = renderer.try_get_skeletal_animator(entity);
                 try_animator.has_value())
        {   // Sanity checks.
            assert(!eds.working_model_name.empty());
            assert(eds.working_afa_ctrls_copy != nullptr);

            auto& internal_animator{ component_internal::Model_animator::extract_internal_animator(
                try_animator.value()) };

            if (eds.working_model_animator != &internal_animator)
            {   // Reset vars.
                afa_agent.working_anim_state_idx = -1;

                // Get animator.
                eds.working_model_animator = &internal_animator;
                assert(eds.working_model_animator != nullptr);

                // Pause animator.
                eds.working_model_animator->set_paused(true);

                // Fill in animator state name to idx map.
                auto const& anim_states{ eds.working_model_animator->get_animator_states() };

                eds.anim_state_name_to_idx_map.clear();
                for (size_t i = 0; i < anim_states.size(); i++)
                {   // Fill in anim state map.
                    eds.anim_state_name_to_idx_map.emplace(anim_states[i].state_name, i);
                }

                // @THEA: this component check reaches into BTZC engine so it's not available from
                //        here, but maybe there could be a way to ensure that it's working????? ig
                //        just making sure this component isn't attached to the entity in the
                //        .btscene, since it's not an automatically added component after all.
                //          -Thea 2026/08/26
                // // Due to manual control in configuring the model animator, ensure that there is no
                // // auto configuration component attached (this exclusion is just a special case for
                // // this editor agent entity).  -Thea 2025/11/16
                // assert(!reg.any_of<component::Anim_frame_action_controller>(entity));

                // Configure anim frame action data.
                // @NOTE: Using the `anim_frame_action_controller` component will configure the
                //        model animator, however, since we want to manually control which AFA
                //        controller is assigned on this dynamic entity, this component is not
                //        attached.
                eds.working_model_animator->configure_anim_frame_action_controls(
                    eds.working_afa_ctrls_copy,
                    BT::UUID_helper::generate_uuid(),  // dummy
                    // { { .queue_name = "_dev_afa_editor_master", .default_is_watching = true } });  @THEA: Idk if it should be like this or not.
                    {});

                // Create and attach hitcapsule set driver.
                // @NOTE: This is also manually added.
                reg.emplace_or_replace<component::Animator_driven_hitcapsule_set>(entity);
            }

            // Update animator state.
            if (afa_agent.working_anim_state_idx != eds.selected_anim_state_idx)
            {
                afa_agent.working_anim_state_idx = eds.selected_anim_state_idx;

                // Set control region idx.
                eds.selected_action_timeline_idx =
                    eds.working_model_animator->get_anim_frame_action_data_handle()
                        .anim_state_idx_to_timeline_idx_map.at(afa_agent.working_anim_state_idx);

                // Set initial animator state.
                eds.working_model_animator->change_state_set(
                    { .anim_state_indices = { afa_agent.working_anim_state_idx },
                      .loop_final_state = false });

                // Set editor state from animator.
                auto const& anim_state{ eds.working_model_animator->get_animator_state(
                    afa_agent.working_anim_state_idx) };
                auto anim_state_anim_idx{ anim_state.state_type == anim_state.SINGLE_ANIM
                                              ? anim_state.animation_idx
                                              : anim_state.blend_anims.front().animation_idx };  // @TEMP
                eds.selected_anim_num_frames =
                    eds.working_model_animator->get_model_animation(anim_state_anim_idx)
                        .get_num_frames();
            }

            // Update animator frame.
            assert(eds.working_model_animator != nullptr);

            auto current_frame_clamped{ eds.anim_current_frame };  // @NOTE: Assumed clamped.
            eds.working_model_animator->set_time(current_frame_clamped /
                                                 k_skeletal_anim_frames_per_second);
            eds.working_model_animator->update(SIMULATION_TIMER_PROFILE, 0);  // Forces data update.

            // Process all controllable data.
            // @NOTE: Just for the editor, it's only necessary to flush all the events.
            static std::vector<anim_frame_action::Controllable_data_label> s_all_data_labels;
            if (s_all_data_labels.empty())
            {   // Add in data labels.
                auto const& all_controllable_data_strs{
                    anim_frame_action::Runtime_controllable_data::get_all_str_labels()
                };
                for (auto& data_str : all_controllable_data_strs)
                {
                    auto data_label{
                        anim_frame_action::Runtime_controllable_data::str_label_to_enum(data_str)
                    };
                    s_all_data_labels.emplace_back(data_label);
                }
            }

            for (auto label : s_all_data_labels)
                if (anim_frame_action::Runtime_controllable_data::get_data_type(label) ==
                    anim_frame_action::Runtime_controllable_data::CTRL_DATA_TYPE_RISING_EDGE_EVENT)
                {
                    (void)anim_frame_action::s_editor_state.working_model_animator
                        ->get_anim_frame_action_data_handle()
                        .get_reeve_data_handle(label)
                        .check_if_rising_edge_occurred();
                }
        }
    }
}
