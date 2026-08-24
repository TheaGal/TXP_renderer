#include "anim_frame_action_editor_content.h"

#include <cmath>
#include <cstddef>


void TXP::editor_content::anim_frame_action_editor_content(bool enter, float_t delta_time)
{
    // @THEA: this should already be taken care of.
    // if (enter)
    // {   // Load up editor-specific scene.
    //     auto& scene_loader{ service_finder::find_service<world::Scene_loader>() };
    //     scene_loader.unload_all_scenes();
    //     scene_loader.load_scene("_dev_animation_editor_view.btscene");

    //     anim_frame_action::s_editor_state = {};  // Reset editor state.
    // }

    static size_t s_selected_afa_idx{ 0 };
    static int32_t s_current_animation_clip{ -1 };  // -1 means unset.
    static auto s_all_afa_names{ anim_frame_action::Bank::get_all_names() };

    // @NOCHECKIN
    // system::imgui_render_transform_hierarchy_window(enter);

    // Timeline selection.
    ImGui::Begin("Timeline select");
    {
        static bool s_load_selected_timeline{ true };

        bool trigger_afa_list_refresh{ ImGui::Button("Refresh list") };

        if (anim_frame_action::s_editor_state.is_editor_state_untouched)
        {   // List refresh will load a timeline, initializing the editor state.
            trigger_afa_list_refresh = true;
            anim_frame_action::s_editor_state.is_editor_state_untouched = false;
        }

        if (trigger_afa_list_refresh)
        {   // Reset timeline selection and load first one from refreshed list.
            s_all_afa_names = anim_frame_action::Bank::get_all_names();
            s_selected_afa_idx = 0;
            s_load_selected_timeline = true;
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Text("Models:%llu Avg:6.9KB Mdn:6.9KB Max:69.4KB", s_all_afa_names.size());

        ImGui::SeparatorText("List of Timelines");

        for (size_t i = 0; i < s_all_afa_names.size(); i++)
        {
            auto& afa_name{ s_all_afa_names[i] };
            bool is_active_afa{ s_selected_afa_idx == i };
            bool is_active_afa_dirty{ anim_frame_action::s_editor_state.is_working_afa_dirty };

            ImGui::BeginDisabled(is_active_afa_dirty || is_active_afa);
            if (ImGui::Button((afa_name
                               + (is_active_afa && is_active_afa_dirty
                                  ? "*"
                                  : "")).c_str()) &&
                !is_active_afa)
            {   // Request loading new model.
                s_selected_afa_idx = i;
                s_load_selected_timeline = true;
            }
            ImGui::EndDisabled();

            if (is_active_afa && is_active_afa_dirty)
            {   // Add save or discard buttons.
                ImGui::SameLine();
                if (ImGui::Button("Save changes"))
                {
                    auto const& afa_name{ s_all_afa_names[s_selected_afa_idx] };
                    {   // Remove hitcapsule group set from overlap solver.
                        // @NOTE: This gets unregistered right before serialization since a new
                        //        working timeline copy is made and that hitcapsule group set gets
                        //        added to the next created animator.
                        auto& hitcapsule_grp_set{
                            anim_frame_action::s_editor_state.working_model_animator
                                ->get_anim_frame_action_data_handle()
                                .hitcapsule_group_set
                        };
                        hitcapsule_grp_set.unregister_from_overlap_solver();

                        // Sort regions so they're sorted when loaded.
                        for (auto& tmln : anim_frame_action::s_editor_state.working_afa_ctrls_copy
                                              ->data.anim_frame_action_timelines)
                        {
                            using Region = anim_frame_action::Runtime_data_controls::Data::
                                Animation_frame_action_timeline::Region;
                            std::sort(tmln.regions.begin(),
                                      tmln.regions.end(),
                                      [](Region const& a, Region const& b) -> bool {
                                          // Really inefficient sorting algo but it's only for an
                                          // editor tool and so fuck it.  -Thea 2026/01/16
                                          if (a.row_idx != b.row_idx)
                                              return a.row_idx < b.row_idx;
                                          if (a.start_frame != b.start_frame)
                                              return a.start_frame < b.start_frame;
                                          if (a.end_frame != b.end_frame)
                                              return a.end_frame < b.end_frame;

                                          // These are equal, so just false it.
                                          return false;
                                      });
                        }

                        // Error if cmd names and/or argv's are invalid.
                        static auto const& k_cmd_docs{
                            Model_animator::get_control_command_codes_documentation()
                        };
                        for (auto const& tmln :
                             anim_frame_action::s_editor_state.working_afa_ctrls_copy->data
                                 .anim_frame_action_timelines)
                        {
                            for (auto const& region : tmln.regions)
                            {
                                auto const& ctrl_cmd{ region.ctrl_cmd };

                                // Ensure that command name exists.
                                using Ctrl_cmd_documentation =
                                    Model_animator::Ctrl_cmd_documentation;
                                Ctrl_cmd_documentation const* found_cmd_doc{ nullptr };

                                for (auto const& cmd_doc : k_cmd_docs)
                                {
                                    if (ctrl_cmd.cmd_name == cmd_doc.cmd.name)
                                    {
                                        found_cmd_doc = &cmd_doc;
                                        break;
                                    }
                                }
                                if (found_cmd_doc == nullptr)
                                {
                                    BT_ERRORF("Could not find control command: %s",
                                              ctrl_cmd.cmd_name.c_str());
                                    assert(false);
                                    abort();
                                }

                                // Ensure that argv count is correct.
                                if (ctrl_cmd.argv.size() != found_cmd_doc->argv.size())
                                {
                                    BT_ERRORF(
                                        "Control command has %llu number of argv instead of "
                                        "required %llu number of argv.",
                                        ctrl_cmd.argv.size(),
                                        found_cmd_doc->argv.size());
                                    assert(false);
                                    abort();
                                }
                            }
                        }

                        // Apply hitcapsule group set to template copy for saving.
                        anim_frame_action::s_editor_state.working_afa_ctrls_copy->data
                            .hitcapsule_group_set_template = hitcapsule_grp_set;

                        // Serialize the working timeline copy.
                        json working_timeline_copy_as_json =
                            anim_frame_action::s_editor_state.working_afa_ctrls_copy->data;

                        // Save to disk.
                        json_save_to_disk(working_timeline_copy_as_json,
                                          BTZC_GAME_ENGINE_ASSET_ANIM_FRAME_ACTIONS_PATH +
                                              afa_name);
                    }

                    anim_frame_action::Bank::replace(
                        afa_name,
                        std::move(*anim_frame_action::s_editor_state.working_afa_ctrls_copy));

                    // Load the same timeline again.
                    s_load_selected_timeline = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard changes"))
                {   // Replace group set with original template to reset changes.
                    anim_frame_action::s_editor_state.working_model_animator
                        ->get_anim_frame_action_data_handle()
                        .hitcapsule_group_set.replace_and_reregister(
                            anim_frame_action::s_editor_state.working_afa_ctrls_copy->data
                                .hitcapsule_group_set_template,
                            anim_frame_action::s_editor_state.working_entity_uuid);

                    // Discard changes by loading the same timeline again.
                    s_load_selected_timeline = true;
                }
            }
        }

        if (s_load_selected_timeline)
        {   // Process load timeline (and model) request.
            if (anim_frame_action::s_editor_state.working_afa_ctrls_copy != nullptr)
                delete anim_frame_action::s_editor_state.working_afa_ctrls_copy;

            anim_frame_action::s_editor_state.working_afa_ctrls_copy =
                new anim_frame_action::Runtime_data_controls(
                    anim_frame_action::Bank::get(s_all_afa_names[s_selected_afa_idx]));
            assert(anim_frame_action::s_editor_state.working_afa_ctrls_copy != nullptr);

            anim_frame_action::s_editor_state.working_model =
                anim_frame_action::s_editor_state.working_afa_ctrls_copy->animated_model;
            assert(anim_frame_action::s_editor_state.working_model != nullptr);

            anim_frame_action::s_editor_state.is_working_afa_dirty = false;  // Load from disk so not dirty.

            // Reset selected indices.
            anim_frame_action::s_editor_state.selected_anim_state_idx = 0;
            anim_frame_action::s_editor_state.selected_action_timeline_idx = 0;
            anim_frame_action::s_editor_state.anim_state_name_to_idx_map.clear();  // Cleared to prevent `s_current_animation_clip` from getting set to the wrong anim state idx immediately.
            s_current_animation_clip = -1;

            s_load_selected_timeline = false;
        }
    }
    ImGui::End();

    // Timeline data viewer.
    ImGui::Begin("Timeline controllable data");
    if (anim_frame_action::s_editor_state.working_model_animator != nullptr)
    {
        auto const& all_controllable_data_strs{ anim_frame_action::Runtime_controllable_data::get_all_str_labels() };
        for (auto& data_str : all_controllable_data_strs)
        {
            auto data_label{ anim_frame_action::Runtime_controllable_data::str_label_to_enum(data_str) };
            switch ( anim_frame_action::Runtime_controllable_data::get_data_type(data_label))
            {
                case anim_frame_action::Runtime_controllable_data::CTRL_DATA_TYPE_FLOAT:
                    ImGui::Text("%s : %0.4f",
                                data_str.c_str(),
                                anim_frame_action::s_editor_state.working_model_animator
                                    ->get_anim_frame_action_data_handle()
                                    .get_float_data_handle(data_label)
                                    .get_val());
                    break;

                case anim_frame_action::Runtime_controllable_data::CTRL_DATA_TYPE_BOOL:
                    ImGui::Text("%s : %s",
                                data_str.c_str(),
                                (anim_frame_action::s_editor_state.working_model_animator
                                     ->get_anim_frame_action_data_handle()
                                    .get_bool_data_handle(data_label)
                                    .get_val()
                                 ? "TRUE"
                                 : "FALSE"));
                    break;

                case anim_frame_action::Runtime_controllable_data::CTRL_DATA_TYPE_RISING_EDGE_EVENT:
                {
                    ImGui::Text("%s : ", data_str.c_str());
                    ImGui::SameLine();
                    auto& reeve_handle{ anim_frame_action::s_editor_state.working_model_animator
                                            ->get_anim_frame_action_data_handle()
                                            .get_reeve_data_handle(data_label) };

                    // >0.0: Trigger has been set off, and returns to 0.0.
                    float_t trigger_lerp_val{ reeve_handle.update_cooldown_and_fetch_val(delta_time) };

                    static auto s_trigger_str_fn = [](float_t t) {
                        assert(t >= 0.0f && t <= 1.0f);
                        int32_t t_over_trigger_length = std::roundf(t * (sizeof("trigger") - 1));
                        switch (t_over_trigger_length)
                        {
                            case 0: return "trigger";
                            case 1: return "Trigger";
                            case 2: return "TRigger";
                            case 3: return "TRIgger";
                            case 4: return "TRIGger";
                            case 5: return "TRIGGer";
                            case 6: return "TRIGGEr";
                            case 7: return "TRIGGER";
                            default: assert(false); return "error_str";
                        }
                    };
                    ImGui::TextColored(ImVec4(glm_lerp(1.0f, 1.0f,   trigger_lerp_val),
                                              glm_lerp(1.0f, 0.914f, trigger_lerp_val),
                                              glm_lerp(1.0f, 0.180f, trigger_lerp_val),
                                              glm_lerp(0.3f, 1.0f,   trigger_lerp_val)),
                                       "%s",
                                       s_trigger_str_fn(trigger_lerp_val));
                    break;
                }

                default:
                    assert(false);
                    break;
            }
        }
    }
    else
    {
        ImGui::Text("No working model animator assigned.");
    }
    ImGui::End();

    // Hitcapsule editor.
    ImGui::Begin("Animator action frame hitcapsule group set param editor");
    if (anim_frame_action::s_editor_state.working_model_animator != nullptr)
    {
        auto& hitcapsule_grp_set{ anim_frame_action::s_editor_state.working_model_animator
                                      ->get_anim_frame_action_data_handle()
                                      .hitcapsule_group_set };

        size_t cap_grp_idx{ 0 };
        size_t global_capsule_id_idx{ 0 };
        for (auto& cap_grp : hitcapsule_grp_set.get_hitcapsule_groups())
        {
            if (ImGui::CollapsingHeader(
                    ("Hitcapsule group #" + std::to_string(cap_grp_idx)).c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen))
            {   // Gui for hitcapsule group.
                ImGui::BeginDisabled();

                bool cap_grp_enabled{ cap_grp.is_enabled() };
                ImGui::Checkbox(("Enabled##" + std::to_string(global_capsule_id_idx)).c_str(),
                                &cap_grp_enabled);

                int32_t cap_grp_type{ cap_grp.get_type() };
                ImGui::Combo(("Type##" + std::to_string(global_capsule_id_idx)).c_str(),
                             &cap_grp_type,
                             "Receive hurt\0Give hurt\0Send aggro signal\0");

                ImGui::EndDisabled();

                ImGui::SeparatorText("Capsules within group");

                size_t cap_idx{ 0 };
                for (auto& capsule : cap_grp.get_capsules())
                {
                    if (ImGui::TreeNodeEx(reinterpret_cast<void*>(global_capsule_id_idx),
                                          0,
                                          "group[%llu].capsules[%llu]",
                                          cap_grp_idx,
                                          cap_idx))
                    {   // Editing params for capsules.
                        if (ImGui::DragFloat3(
                                ("origin_a##hitcapsule_grp_set_hitcapsule_grp_hitcapsule" +
                                 std::to_string(global_capsule_id_idx))
                                    .c_str(),
                                capsule.origin_a.raw,
                                0.0125f))
                        {
                            glm_vec3_copy(capsule.origin_a.raw, capsule.calcd_origin_a);
                            anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                        }

                        if (ImGui::DragFloat3(
                                ("origin_b##hitcapsule_grp_set_hitcapsule_grp_hitcapsule" +
                                 std::to_string(global_capsule_id_idx))
                                    .c_str(),
                                capsule.origin_b.raw,
                                0.0125f))
                        {
                            glm_vec3_copy(capsule.origin_b.raw, capsule.calcd_origin_b);
                            anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                        }

                        if (ImGui::DragFloat(
                                ("radius##hitcapsule_grp_set_hitcapsule_grp_hitcapsule" +
                                 std::to_string(global_capsule_id_idx))
                                    .c_str(),
                                &capsule.radius,
                                0.0125f))
                        {
                            anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                        }

                        ImGui::TreePop();
                    }

                    cap_idx++;
                    global_capsule_id_idx++;
                }
            }

            cap_grp_idx++;
        }
    }
    else
    {
        ImGui::Text("No working model animator assigned.");
    }
    ImGui::End();

    // Animation timeline.
    ImGui::Begin("Animation timeline",
                 nullptr,
                 (anim_frame_action::s_editor_state.is_working_afa_dirty
                      ? ImGuiWindowFlags_UnsavedDocument
                      : 0));
    {
        // Fill out anim state names.
        static std::vector<std::string> s_anim_names_as_list;

        // Set animation clip idx to currently set selected anim state idx.
        if (s_current_animation_clip == -1 &&
            !anim_frame_action::s_editor_state.anim_state_name_to_idx_map.empty())
        {
            s_anim_names_as_list.clear();
            s_anim_names_as_list.reserve(
                anim_frame_action::s_editor_state.anim_state_name_to_idx_map.size());

            size_t new_clip_idx{ 0 };
            for (auto&& [anim_name, idx] : anim_frame_action::s_editor_state.anim_state_name_to_idx_map)
            {
                s_anim_names_as_list.emplace_back(anim_name);

                if (idx == anim_frame_action::s_editor_state.selected_anim_state_idx)
                {   // Set found idx.
                    s_current_animation_clip = new_clip_idx;
                }

                new_clip_idx++;
            }
        }

        // Listbox of anim state names.
        {
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();

            ImGui::BeginDisabled(anim_frame_action::s_editor_state.is_working_afa_dirty);

            // Change selected anim idx in editor state.
            if (custom_imgui_listbox("anim_state_name_listbox",
                                     canvas_size.x * 0.3f,
                                     canvas_size.y,
                                     s_anim_names_as_list,
                                     s_current_animation_clip))
            {
                anim_frame_action::s_editor_state.selected_anim_state_idx =
                    anim_frame_action::s_editor_state.anim_state_name_to_idx_map.at(
                        s_anim_names_as_list[s_current_animation_clip]);
            }

            // Tooltip: cannot switch anim states while working AFA timeline is dirty.
            if (anim_frame_action::s_editor_state.is_working_afa_dirty &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {   // Set tooltip if disabled.
                ImGui::SetTooltip(
                    "Working timeline is dirty. Changing animation clip\n"
                    "is disabled until changes are saved or discarded.");
            }

            ImGui::EndDisabled();

            // For upcoming custom widget.
            ImGui::SameLine();
        }

        // Region cmd editing popup.
        struct Cmd_edit_data
        {
            using Control_command = anim_frame_action::Runtime_data_controls::Data::
                Animation_frame_action_timeline::Region::Control_command;
            bool not_been_accessed{ false };
            Control_command ctrl_cmd_copy;
            Control_command* write_ptr{ nullptr };
        };
        static Cmd_edit_data s_cmd_edit_popup_data;

        if (!ImGui::IsPopupOpen("region_cmd_edit_popup") &&
            s_cmd_edit_popup_data.write_ptr != nullptr)
        {
            if (s_cmd_edit_popup_data.not_been_accessed)
            {
                s_cmd_edit_popup_data.not_been_accessed = false;
                ImGui::OpenPopup("region_cmd_edit_popup");
            }
            else
                s_cmd_edit_popup_data.write_ptr = nullptr;
        }
        if (ImGui::BeginPopup("region_cmd_edit_popup"))
        {
            // Cmd list.
            static auto const& k_cmd_docs{
                Model_animator::get_control_command_codes_documentation()
            };
            static auto const k_cmd_list_as_zero_term_str_fn = []() {  // @TODO: this is a useful little func!! @THEA
                size_t n{ 0 };
                for (auto const& cmd_doc : k_cmd_docs)
                    n += cmd_doc.cmd.name.size() + 1;  // +1 for \0

                std::string zts(n, '\0');
                size_t i{ 0 };
                for (auto const& cmd_doc : k_cmd_docs)
                {
                    for (auto cmd_str_char : cmd_doc.cmd.name)
                    {
                        zts.at(i) = cmd_str_char;
                        i++;
                    }
                    i++;  // +1 for \0
                }

                return zts;
            };
            static std::string const k_cmd_list_as_zero_term_str{
                k_cmd_list_as_zero_term_str_fn()
            };

            // Find cmd idx from string.
            int32_t cmd_idx{ -1 };
            size_t i{ 0 };
            for (auto const& cmd_doc : k_cmd_docs)
            {
                if (cmd_doc.cmd.name == s_cmd_edit_popup_data.ctrl_cmd_copy.cmd_name)
                {
                    cmd_idx = i;
                    break;
                }
                i++;
            }
            assert(cmd_idx >= 0);

            if (ImGui::Combo("Command", &cmd_idx, k_cmd_list_as_zero_term_str.c_str()))
            {   // Switch to selected cmd.
                s_cmd_edit_popup_data.ctrl_cmd_copy.cmd_name = k_cmd_docs[cmd_idx].cmd.name;
                s_cmd_edit_popup_data.ctrl_cmd_copy.argv.resize(k_cmd_docs[cmd_idx].argv.size());
            }

            // Separator.
            ImGui::SeparatorText(
                ("Arguments (" + std::to_string(k_cmd_docs[cmd_idx].argv.size()) + ")").c_str());

            // Fill in command args.
            assert(k_cmd_docs[cmd_idx].argv.size() ==
                   s_cmd_edit_popup_data.ctrl_cmd_copy.argv.size());
            for (size_t i = 0; i < k_cmd_docs[cmd_idx].argv.size(); i++)
            {
                auto const& cmd_arg{ k_cmd_docs[cmd_idx].argv[i] };
                auto& editing_cmd_arg{ s_cmd_edit_popup_data.ctrl_cmd_copy.argv[i] };

                // Use different editor depending on type (tho underlying is str).
                auto lbl_txt{ cmd_arg.name + ": " + cmd_arg.type };
                if (cmd_arg.type == "str")
                {
                    ImGui::InputText(lbl_txt.c_str(), &editing_cmd_arg);
                }
                else if (cmd_arg.type == "int")
                {
                    int32_t arg_val{ std::stoi(editing_cmd_arg) };
                    if (ImGui::InputInt(lbl_txt.c_str(), &arg_val))
                        editing_cmd_arg = std::to_string(arg_val);
                }
                else if (cmd_arg.type == "float")
                {
                    float_t arg_val{ std::stof(editing_cmd_arg) };
                    if (ImGui::InputFloat(lbl_txt.c_str(), &arg_val))
                        editing_cmd_arg = std::to_string(arg_val);
                }
            }

            // Confirm/Cancel buttons.
            ImGui::Separator();

            if (ImGui::Button("Confirm") ||
                m_input_handler->is_key_pressed(BT_KEY_ENTER))
            {   // Submit rename.
                *s_cmd_edit_popup_data.write_ptr = s_cmd_edit_popup_data.ctrl_cmd_copy;

                anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel") ||
                m_input_handler->is_key_pressed(BT_KEY_ESCAPE))
            {   // Cancel!!!
                ImGui::CloseCurrentPopup();
            }

            ImGui::Text("%s", "Press <Enter> to confirm rename or <Esc> to cancel.");

            ImGui::EndPopup();
        }

        // BT sequencer widget.
        ImGui::BeginChild("BT_sequencer");
        if (anim_frame_action::s_editor_state.anim_state_name_to_idx_map.empty())
        {
            ImGui::SetWindowFontScale(5.0f);

            // Just show empty message.
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(209/256.0, 186/256.0, 73/256.0, 1));
            ImGui::TextWrapped("Selected model \"%s\" does not contain any animations.",
                               s_all_afa_names[s_selected_afa_idx].c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::SetWindowFontScale(1.0f);

            // Sequencer controls.
            static int32_t s_current_frame = 0;
            static int32_t s_final_frame = 60;

            anim_frame_action::s_editor_state.anim_current_frame =  // A frame late but oh well.
                std::min(std::max(0, s_current_frame), s_final_frame);

            ImGui::PushItemWidth(130);

            ImGui::Text("Displaying frame %llu/%d. %.2f FPS",
                        anim_frame_action::s_editor_state.anim_current_frame,
                        s_final_frame,
                        Model_joint_animation::k_frames_per_second);
            ImGui::InputInt("Selected Frame", &s_current_frame);

            ImGui::PopItemWidth();






            ////////////////////////////////////////////////////////////////////////////////////////





            auto& afa_timeline_regions{
                anim_frame_action::s_editor_state.working_afa_ctrls_copy->data
                    .anim_frame_action_timelines[anim_frame_action::s_editor_state
                                                     .selected_action_timeline_idx]
                    .regions
            };

            static vec2s s_timeline_cell_size{ 16.0f, 24.0f };

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 canvas_pos = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();  // Resize canvas to what's available.

            static float_t s_sequencer_x_offset{ 0.0f };
            static float_t s_sequencer_y_offset{ 0.0f };

            // Clip rects.
            ImVec2 cr_timeline_min{ canvas_pos };
            ImVec2 cr_timeline_max{ canvas_pos.x + canvas_size.x,
                                    canvas_pos.y + canvas_size.y };

            constexpr int32_t k_top_measuring_region_height{ 20 };

            // Sequencer timeline.
            ImGui::PushClipRect(cr_timeline_min, cr_timeline_max, true);
            {   // Draw bg.
                draw_list->AddRectFilled(cr_timeline_min, cr_timeline_max, 0xFF2D2D2D);

                if (ImGui::IsWindowHovered() &&
                    ImGui::IsMouseHoveringRect(cr_timeline_min,
                                               cr_timeline_max))
                {   // Scrolling behavior.
                    auto& ins{ m_input_handler->get_input_state() };
                    if (ins.le_lctrl_mod.val)
                    {   // @TODO: Add focusing onto where the mouse cursor is instead of global cell size.
                        s_timeline_cell_size.x +=
                            m_input_handler->get_input_state().ui_scroll_delta.val
                            * 1.5f;
                    }
                    else
                    {
                        (ins.le_lshift_mod.val ? s_sequencer_x_offset : s_sequencer_y_offset) +=
                            m_input_handler->get_input_state().ui_scroll_delta.val * 40.0f;
                    }
                }

                constexpr size_t k_num_timeline_rows{ 100 };
                for (size_t i = 0; i < k_num_timeline_rows; i++)
                {
                    vec2s y_top_btm;
                    {
                        y_top_btm.s = (cr_timeline_min.y + s_sequencer_y_offset + k_top_measuring_region_height + 2 + (s_timeline_cell_size.y * i));
                        y_top_btm.t = (y_top_btm.s + s_timeline_cell_size.y);
                    }

                    if (i % 2 == 1)
                    {   // Draw bg for odd rows.
                        draw_list->AddRectFilled(ImVec2{ cr_timeline_min.x, y_top_btm.s },
                                                 ImVec2{ cr_timeline_max.x, y_top_btm.t },
                                                 0x11DDDDDD);
                    }

                    if (i == 0)
                    {   // Draw above line.
                        draw_list->AddLine(ImVec2{ cr_timeline_min.x, y_top_btm.s },
                                           ImVec2{ cr_timeline_max.x, y_top_btm.s },
                                           0x99DDDDDD);
                    }

                    if (i == k_num_timeline_rows - 1)
                    {   // Draw below line.
                        draw_list->AddLine(ImVec2{ cr_timeline_min.x, y_top_btm.t },
                                           ImVec2{ cr_timeline_max.x, y_top_btm.t },
                                           0x99DDDDDD);
                    }
                }

                // Region selecting.
                struct Region_selecting
                {
                    enum Select_state
                    {
                        UNSELECTED,
                        SELECTED,
                        LEFT_DRAG,
                        WHOLE_DRAG,
                        RIGHT_DRAG,
                    };
                    Select_state sel_state{ Select_state::UNSELECTED };
                    using Region = anim_frame_action::Runtime_data_controls::Data::
                        Animation_frame_action_timeline::Region;
                    Region* sel_reg{ nullptr };
                    float_t drag_x_amount{ 0.0f };
                    float_t drag_y_amount{ 0.0f };
                    bool prev_lmb_pressed{ false };
                    bool prev_rmb_pressed{ false };
                    bool prev_del_pressed{ false };
                };
                static Region_selecting s_reg_sel;

                // Selecting inputs.
                bool cur_lmb_pressed{ m_input_handler->get_input_state().le_select.val };
                bool on_lmb_press{ cur_lmb_pressed && !s_reg_sel.prev_lmb_pressed };
                bool on_lmb_release{ !cur_lmb_pressed && s_reg_sel.prev_lmb_pressed };
                s_reg_sel.prev_lmb_pressed = cur_lmb_pressed;

                bool cur_rmb_pressed{ m_input_handler->get_input_state().le_rclick_cam.val };
                bool on_rmb_press{ cur_rmb_pressed && !s_reg_sel.prev_rmb_pressed };
                bool on_rmb_release{ !cur_rmb_pressed && s_reg_sel.prev_rmb_pressed };
                s_reg_sel.prev_rmb_pressed = cur_rmb_pressed;

                bool cur_del_pressed{ m_input_handler->is_key_pressed(BT_KEY_DELETE) ||
                                      m_input_handler->is_key_pressed(BT_KEY_X) };
                bool on_del_press{ cur_del_pressed && !s_reg_sel.prev_del_pressed };
                s_reg_sel.prev_del_pressed = cur_del_pressed;

                if (s_reg_sel.sel_reg != nullptr)
                {   // Drag region (horizontal).
                    s_reg_sel.drag_x_amount += m_input_handler->get_input_state()
                                               .look_delta.x.val;
                    while (abs(s_reg_sel.drag_x_amount) > s_timeline_cell_size.x * 0.5f)
                    {   // Modulate dragged amount and apply to dragging region.
                        int32_t drag_sign{
                            static_cast<int32_t>(glm_signf(s_reg_sel.drag_x_amount)) };
                        s_reg_sel.drag_x_amount -= (s_timeline_cell_size.x
                                                    * drag_sign);

                        bool left_side_drag{ false };
                        if (s_reg_sel.sel_state == Region_selecting::LEFT_DRAG ||
                            s_reg_sel.sel_state == Region_selecting::WHOLE_DRAG)
                        {   // Left side drag.
                            s_reg_sel.sel_reg->start_frame += drag_sign;
                            left_side_drag = true;
                        }
                        if (s_reg_sel.sel_state == Region_selecting::RIGHT_DRAG ||
                            s_reg_sel.sel_state == Region_selecting::WHOLE_DRAG)
                        {   // Right side drag.
                            s_reg_sel.sel_reg->end_frame += drag_sign;
                            left_side_drag = false;
                        }

                        // Check for overlap issue/error after all drag operations.
                        if (left_side_drag)
                        {
                            s_reg_sel.sel_reg->start_frame =
                                glm_min(s_reg_sel.sel_reg->start_frame,
                                        s_reg_sel.sel_reg->end_frame - 1);
                        }
                        else
                        {
                            s_reg_sel.sel_reg->end_frame =
                                glm_max(s_reg_sel.sel_reg->start_frame + 1,
                                        s_reg_sel.sel_reg->end_frame);
                        }

                        // Mark working timeline as dirty.
                        anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                    }

                    if (s_reg_sel.sel_state == Region_selecting::WHOLE_DRAG)
                    {   // Drag region (vertical).
                        s_reg_sel.drag_y_amount +=
                            m_input_handler->get_input_state().look_delta.y.val;
                        while (abs(s_reg_sel.drag_y_amount) > s_timeline_cell_size.y * 0.5f)
                        {   // Modulate dragged amount and apply to dragging region.
                            int32_t drag_sign{
                                static_cast<int32_t>(glm_signf(s_reg_sel.drag_y_amount)) };
                            s_reg_sel.drag_y_amount -= (s_timeline_cell_size.y * drag_sign);

                            // Move row depending on drag direction.
                            s_reg_sel.sel_reg->row_idx += drag_sign;

                            // Mark working timeline as dirty.
                            anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                        }
                    }

                    if (on_lmb_release)
                    {   // Release drag.
                        s_reg_sel.sel_state = Region_selecting::SELECTED;
                    }

                    if (on_lmb_press)
                    {   // Deselect selected region.
                        s_reg_sel.sel_state = Region_selecting::UNSELECTED;
                        s_reg_sel.sel_reg = nullptr;
                    }
                }

                // Detect whether cursor is over empty area on timeline.
                bool is_hovering_over_timeline{
                    ImGui::IsWindowHovered() &&
                    ImGui::IsMouseHoveringRect(ImVec2(cr_timeline_min.x, cr_timeline_min.y + glm_max(0, s_sequencer_y_offset) + k_top_measuring_region_height + 2),
                                               ImVec2(cr_timeline_max.x, glm_min(cr_timeline_max.y, cr_timeline_min.y + s_sequencer_y_offset + k_top_measuring_region_height + 2 + (s_timeline_cell_size.y * k_num_timeline_rows)))) };
                bool is_hovering_over_timeline_region{ false };  // Check in upcoming block.

                for (auto& region : afa_timeline_regions)
                {   // Check that the sequencer current frame has this region active.
                    bool is_active_this_frame{ s_current_frame >= region.start_frame &&
                                               s_current_frame < region.end_frame };

                    // Draw bars for regions.
                    vec2s region_bar_top_bottom{
                        cr_timeline_min.y + s_sequencer_y_offset + k_top_measuring_region_height + 2 + (s_timeline_cell_size.y * region.row_idx) + 1,
                        cr_timeline_min.y + s_sequencer_y_offset + k_top_measuring_region_height + 2 + (s_timeline_cell_size.y * (region.row_idx + 1)) - 1 };
                    ImVec2 p_min{ cr_timeline_min.x + s_sequencer_x_offset + (region.start_frame * s_timeline_cell_size.x) + 1, region_bar_top_bottom.s };
                    ImVec2 p_max{ cr_timeline_min.x + s_sequencer_x_offset + (region.end_frame * s_timeline_cell_size.x) - 1, region_bar_top_bottom.t };
                    draw_list->AddRectFilled(p_min,
                                             p_max,
                                             (is_active_this_frame ? 0x5500FF00 : 0x556DFC6D),
                                             4.0f);

                    bool is_selected_region{ &region == s_reg_sel.sel_reg };
                    draw_list->AddRect(p_min,
                                       p_max,
                                       (is_selected_region ? 0xFF3176F5 : 0x55FFFFFF),
                                       4.0f,
                                       NULL,
                                       (is_selected_region ? 4.0f : 2.0f));

                    // Draw text inside bar.
                    std::stringstream complete_cmd;
                    complete_cmd << region.ctrl_cmd.cmd_name << "(";

                    bool first{ true };
                    for (auto const& a : region.ctrl_cmd.argv)
                    {
                        complete_cmd << (first ? "" : ", ") << a;
                        first = false;
                    }
                    complete_cmd << ")";

                    constexpr ImVec2 k_cmd_txt_padding{ 4, 4 };
                    draw_list->AddText(
                        ImVec2(p_min.x + k_cmd_txt_padding.x, p_min.y + k_cmd_txt_padding.y),
                        0xFFCCCCCC,
                        complete_cmd.str().c_str());

                    // Open the cmd editing popup.
                    bool open_cmd_edit_popup{ false };

                    // Adjustment handles.
                    constexpr int32_t k_side_handle_size{ 4 };
                    if (ImGui::IsWindowHovered() &&
                        is_hovering_over_timeline &&
                        ImGui::IsMouseHoveringRect(p_min,
                                                   ImVec2(p_min.x + k_side_handle_size, p_max.y)))
                    {   // Left side.
                        is_hovering_over_timeline_region = true;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (on_lmb_press)
                        {
                            s_reg_sel.sel_state = Region_selecting::LEFT_DRAG;
                            s_reg_sel.sel_reg = &region;
                            s_reg_sel.drag_x_amount = 0.0f;
                        }
                        else if (on_rmb_press)
                        {
                            open_cmd_edit_popup = true;
                        }
                    }
                    else if (ImGui::IsWindowHovered() &&
                             is_hovering_over_timeline &&
                             ImGui::IsMouseHoveringRect(ImVec2(p_min.x + k_side_handle_size, p_min.y),
                                                        ImVec2(p_max.x - k_side_handle_size, p_max.y)))
                    {   // Move region.
                        is_hovering_over_timeline_region = true;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        if (on_lmb_press)
                        {
                            s_reg_sel.sel_state = Region_selecting::WHOLE_DRAG;
                            s_reg_sel.sel_reg = &region;
                            s_reg_sel.drag_x_amount = 0.0f;
                            s_reg_sel.drag_y_amount = 0.0f;
                        }
                        else if (on_rmb_press)
                        {
                            open_cmd_edit_popup = true;
                        }
                    }
                    else if (ImGui::IsWindowHovered() &&
                             is_hovering_over_timeline &&
                             ImGui::IsMouseHoveringRect(ImVec2(p_max.x - k_side_handle_size, p_min.y),
                                                        ImVec2(p_max)))
                    {   // Right side.
                        is_hovering_over_timeline_region = true;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (on_lmb_press)
                        {
                            s_reg_sel.sel_state = Region_selecting::RIGHT_DRAG;
                            s_reg_sel.sel_reg = &region;
                            s_reg_sel.drag_x_amount = 0.0f;
                        }
                        else if (on_rmb_press)
                        {
                            open_cmd_edit_popup = true;
                        }
                    }

                    // Open cmd edit popup (cont.)
                    if (open_cmd_edit_popup)
                    {
                        // Setting this val triggers opening edit popup.
                        s_cmd_edit_popup_data.not_been_accessed = true;
                        s_cmd_edit_popup_data.ctrl_cmd_copy = region.ctrl_cmd;
                        s_cmd_edit_popup_data.write_ptr = &region.ctrl_cmd;
                    }
                }

                // Draw measuring region bg.
                draw_list->AddRectFilled(cr_timeline_min, ImVec2(cr_timeline_max.x, cr_timeline_min.y + k_top_measuring_region_height + 2), 0x99000000);

                // Draw measuring lines and numbers.
                constexpr int32_t k_frame_start{ 0 };
                s_final_frame =
                    anim_frame_action::s_editor_state.selected_anim_num_frames;

                for (int32_t i = k_frame_start; i <= s_final_frame; i++)
                {
                    float_t line_x{ cr_timeline_min.x + s_sequencer_x_offset + (i * s_timeline_cell_size.x) };

                    // Draw top measuring line.
                    constexpr std::array<float_t, 10> k_baseline_heights{
                        0, 15, 15, 15, 15,
                        10, 15, 15, 15, 15,
                    };
                    draw_list->AddLine(ImVec2(line_x, cr_timeline_min.y + k_baseline_heights[i % 10]),
                                       ImVec2(line_x, cr_timeline_min.y + k_top_measuring_region_height),
                                       0xFFFFFFFF);

                    if (i == k_frame_start || i == s_final_frame)
                    {   // Draw full height start-end lines.
                        draw_list->AddLine(ImVec2(line_x, cr_timeline_min.y + k_top_measuring_region_height),
                                           ImVec2(line_x, cr_timeline_max.y),
                                           0x77DDDDDD);
                    }

                    if (i % 10 == 0)
                    {   // Draw 10s benchmark number.
                        auto number_label{ std::to_string(i) };
                        // @TODO: Fix the offset. Look into how it's done in `imgui_renderer.cpp` and stuff.
                        draw_list->AddText(ImVec2(cr_timeline_min.x
                                                  + s_sequencer_x_offset
                                                  + (i * s_timeline_cell_size.x)
                                                  + 4,
                                                  cr_timeline_min.y + 0),
                                           0xFFFFFFFF,
                                           number_label.c_str());
                    }
                }

                {   // Draw current frame line.
                    auto cur_frame_str{ std::to_string(s_current_frame) };
                    float_t cur_frame_line_x{ cr_timeline_min.x + s_sequencer_x_offset + (s_current_frame * s_timeline_cell_size.x) };

                    draw_list->AddLine(ImVec2(cur_frame_line_x, cr_timeline_min.y + 0),
                                       ImVec2(cur_frame_line_x, cr_timeline_max.y),
                                       0xFF7A50FA,
                                       2.0f);
                    draw_list->AddRectFilled(ImVec2(cur_frame_line_x, cr_timeline_min.y + 0),
                                             ImVec2(cur_frame_line_x + 4 + ImGui::CalcTextSize(cur_frame_str.c_str()).x + 4, cr_timeline_min.y + k_top_measuring_region_height),
                                             0xFF7A50FA);
                    draw_list->AddText(ImVec2(cur_frame_line_x + 4, cr_timeline_min.y + 2),
                                       0xFFFFFFFF,
                                       cur_frame_str.c_str());
                }

                // For click-n-drag.
                static bool s_move_current_frame_to_mouse_active{ false };
                if (on_lmb_release)
                    s_move_current_frame_to_mouse_active = false;

                if (!cur_lmb_pressed &&
                    on_del_press &&
                    s_reg_sel.sel_state == Region_selecting::SELECTED)
                {   // Delete selected region.
                    assert(s_reg_sel.sel_reg != nullptr);
                    for (size_t i = afa_timeline_regions.size() - 1;; i--)
                    {
                        if (&afa_timeline_regions[i] == s_reg_sel.sel_reg)
                        {   // Found the one to delete.
                            afa_timeline_regions.erase(afa_timeline_regions.begin() + i);
                            break;
                        }
                        if (i == 0)
                        {   // Searching failed. Abort/exit.
                            logger::printe(logger::ERROR,
                                           "Delete selected region searching failed.");
                            assert(false);
                            break;
                        }
                    }

                    // Clear selection state.
                    // (Do this right after to prevent stale pointer issues)
                    s_reg_sel.sel_state = Region_selecting::UNSELECTED;
                    s_reg_sel.sel_reg = nullptr;

                    // Mark working timeline as dirty.
                    anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                }
                else if ((s_move_current_frame_to_mouse_active && cur_lmb_pressed) ||
                         (on_lmb_press && ImGui::IsWindowHovered() &&
                          ImGui::IsMouseHoveringRect(
                              ImVec2(cr_timeline_min),
                              ImVec2(cr_timeline_max.x,
                                     cr_timeline_min.y + k_top_measuring_region_height + 2))))
                {   // Move current frame to mouse.
                    s_move_current_frame_to_mouse_active = true;
                    float_t zoom_relative_mouse_x{ (ImGui::GetIO().MousePos.x
                                                    - (cr_timeline_min.x + s_sequencer_x_offset))
                                                   / s_timeline_cell_size.x };
                    s_current_frame = std::roundf(zoom_relative_mouse_x);
                }
                else if (is_hovering_over_timeline && is_hovering_over_timeline_region)
                {
                    ImGui::SetTooltip("Left click to select/move.\n"
                                      "Right click to edit region\'s cmd.");

                    // @NOTE: LMB and RMB inputs are handled when inside the region's drawing code.
                }
                else if (is_hovering_over_timeline &&
                         !is_hovering_over_timeline_region &&
                         s_reg_sel.sel_state <= Region_selecting::SELECTED)  // Not doing a drag operation.
                {   // Prompt creating new region w/ tooltip.
                    ImGui::SetTooltip("Press Shift+A to create new region.");

                    static bool s_prev_is_key_a_pressed{ false };
                    bool cur_is_key_a_pressed{ m_input_handler->is_key_pressed(BT_KEY_A) };
                    if (cur_is_key_a_pressed &&
                        !s_prev_is_key_a_pressed &&
                        m_input_handler->is_key_pressed(BT_KEY_LEFT_SHIFT))
                    {   // Create new region since empty space selected.
                        ImVec2 mouse_pos{ ImGui::GetIO().MousePos };
                        float_t zoom_relative_mouse_x{ (mouse_pos.x
                                                        - (cr_timeline_min.x + s_sequencer_x_offset))
                                                       / s_timeline_cell_size.x };
                        uint32_t hover_row_idx{
                            static_cast<uint32_t>((mouse_pos.y
                                                   - (cr_timeline_min.y
                                                      + s_sequencer_y_offset
                                                      + k_top_measuring_region_height + 2))
                                                  / s_timeline_cell_size.y) };
                        int32_t start_frame{
                            static_cast<int32_t>(std::floorf(zoom_relative_mouse_x)) };

                        afa_timeline_regions.emplace_back(hover_row_idx,
                                                          start_frame,
                                                          start_frame + 4);
                        afa_timeline_regions.back().ctrl_cmd.cmd_name = "nop";  // Default, no-op command.
                        afa_timeline_regions.back().ctrl_cmd.argv.clear();

                        // Immediately assign created region as selected.
                        // (Just in case there may be some kind of vector resizing
                        //  which makes the pointers stale. I hate this issue too)
                        s_reg_sel.sel_state = Region_selecting::SELECTED;
                        s_reg_sel.sel_reg = &afa_timeline_regions.back();

                        // Mark working timeline as dirty.
                        anim_frame_action::s_editor_state.is_working_afa_dirty = true;
                    }

                    s_prev_is_key_a_pressed = cur_is_key_a_pressed;
                }
            }
            ImGui::PopClipRect();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}