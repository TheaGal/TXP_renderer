#include "_dev_animation_frame_action_editor.h"

#include "animation_frame_action_tool/editor_state.h"
#include "animation_frame_action_tool/runtime_data.h"
#include "game_system_logic/component/anim_frame_action_controller.h"
#include "game_system_logic/component/animator_driven_hitcapsule_set.h"
#include "game_system_logic/entity_container.h"
#include "game_system_logic/component/_dev_animation_frame_action_editor_agent.h"
#include "service_finder/service_finder.h"

#include <cassert>


void TXP::system::_dev_animation_frame_action_editor()
{
    auto& entity_container{ service_finder::find_service<Entity_container>() };
    auto& reg{ entity_container.get_ecs_registry() };
    auto view{ reg.view<component::_Dev_animation_frame_action_editor_agent>() };

    // This check is to ensure that editing the editor state will be used/practical.
    assert(view->size() <= 1);

    for (auto&& [entity, afa_agent] : view->each())
    {   // Completely reset editor data.
        // @NOTE: This is the case where the scene for the editor is loaded in, which default
        //        behavior is to load in with flag to reset the editor state.
        if (afa_agent.request_reset_editor_state)
        {
            anim_frame_action::reset_editor_state();
            afa_agent.request_reset_editor_state = false;
        }

        auto& eds{ anim_frame_action::s_editor_state };

        // Reset editor data when working model is changed.
        if (afa_agent.prev_working_model != eds.working_model)
        {
            eds.working_entity_uuid = entity_container.find_entity_uuid(entity);
            eds.working_model_animator = nullptr;
            afa_agent.prev_working_afa_ctrls_copy = nullptr;  // Forces animator reconfiguration.

            // Create render object settings component (to trigger creating a render object).
            reg.emplace_or_replace<component::Render_object_settings>(
                entity,
                Render_layer::RENDER_LAYER_DEFAULT,
                eds.working_model->get_model_name(),
                !eds.working_model->get_joint_animations().empty(),
                eds.working_model->get_model_name() + ".btanitor");

            // Removes prev created rend obj if exists, so that changed settings get regenerated.
            reg.remove<component::Created_render_object_reference>(entity);

            afa_agent.prev_working_model = eds.working_model;
        }

        // Configuration once render object is created.
        if (reg.any_of<component::Created_render_object_reference>(entity))
        {   // Sanity checks.
            assert(eds.working_model != nullptr);
            assert(eds.working_afa_ctrls_copy != nullptr);

            if (eds.working_model_animator == nullptr ||
                afa_agent.prev_working_afa_ctrls_copy != eds.working_afa_ctrls_copy)
            {   // Reset vars.
                afa_agent.working_anim_state_idx = -1;

                // Get animator.
                auto rend_obj_uuid{
                    reg.get<component::Created_render_object_reference>(entity).render_obj_uuid_ref
                };
                auto& rend_obj_pool{
                    service_finder::find_service<Renderer>().get_render_object_pool()
                };
                auto render_obj{
                    rend_obj_pool.checkout_render_obj_by_key({ rend_obj_uuid }).front()
                };

                eds.working_model_animator = render_obj->get_model_animator();
                assert(eds.working_model_animator != nullptr);

                rend_obj_pool.return_render_objs({ render_obj });

                // Pause animator.
                eds.working_model_animator->set_paused(true);

                // Fill in animator state name to idx map.
                auto const& anim_states{ eds.working_model_animator->get_animator_states() };

                eds.anim_state_name_to_idx_map.clear();
                for (size_t i = 0; i < anim_states.size(); i++)
                {   // Fill in anim state map.
                    eds.anim_state_name_to_idx_map.emplace(anim_states[i].state_name, i);
                }

                // Due to manual control in configuring the model animator, ensure that there is no
                // auto configuration component attached (this exclusion is just a special case for
                // this editor agent entity).  -Thea 2025/11/16
                assert(!reg.any_of<component::Anim_frame_action_controller>(entity));

                // Configure anim frame action data.
                // @NOTE: Using the `anim_frame_action_controller` component will configure the
                //        model animator, however, since we want to manually control which AFA
                //        controller is assigned on this dynamic entity, this component is not
                //        attached.
                eds.working_model_animator->configure_anim_frame_action_controls(
                    eds.working_afa_ctrls_copy,
                    eds.working_entity_uuid,
                    // { { .queue_name = "_dev_afa_editor_master", .default_is_watching = true } });  @THEA: Idk if it should be like this or not.
                    {});

                // Create and attach hitcapsule set driver.
                // @NOTE: This is also manually added.
                reg.emplace_or_replace<component::Animator_driven_hitcapsule_set>(entity);

                // Keep track so that if the working timeline gets saved/discarded, a new one is
                // immediately fetched.
                afa_agent.prev_working_afa_ctrls_copy = eds.working_afa_ctrls_copy;
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
                                                 Model_joint_animation::k_frames_per_second);

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
