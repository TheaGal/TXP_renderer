#include "editor_content.h"

#if TXP_GFX_BACKEND_VULKAN
#include "backends/imgui_impl_vulkan.h"
#endif // TXP_GFX_BACKEND_VULKAN

#include "animation_frame_action/editor_state.h"
#include "btlogger.h"
#include "btuuid.h"
#include "camera/camera_internal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"
#include "renderer/gfx.h"
#include "renderer/types.h"
#include "txp_renderer/input_handler/input_handler.h"
#include "txp_renderer/input_handler/input_key_codes.h"
#include "txp_renderer/renderer.h"
#include "txp_renderer/types.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace
{

bool s_show_demo_window{ false };

std::function<void()> s_imgui_build_contents_callback_fn{ nullptr };

struct Manipulate_transform
{
    mat4 transform;
    std::function<void(mat4 const)> manipulated_callback;
};
std::vector<Manipulate_transform> s_manipulate_transform_list;

/// Prompt overlay for a camera mode to get unlocked or toggled with shift+c.
void imgui_camera_mode_shift_c_ctrl_prompt_overlay(
    TXP::Input::Input_handler const& input_handler,
    std::function<void()> const& press_shift_c_fn,
    std::string const& prompt_text,
    ImVec2 topleft_pos)
{
    constexpr float_t k_padding{ 10.0f };
    ImGui::SetNextWindowPos(ImVec2(topleft_pos.x + k_padding, topleft_pos.y + k_padding),
                            ImGuiCond_Always,
                            ImVec2(0, 0));

    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::BeginChild(ImGui::GetID("Camera cursor control prompt overlay"),
                          ImVec2(0, 0),
                          ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX |
                              ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                              ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("%s", prompt_text.c_str());

        static size_t s_prev_key_c_event_tick{ 0 };
        if (auto ks = input_handler.get_keyboard_key_state(BT_KEY_C);
            ks.last_event_tick > s_prev_key_c_event_tick)
        {
            if (ks.pressed && ks.modbits.has_shift())
            {
                press_shift_c_fn();
            }
            s_prev_key_c_event_tick = ks.last_event_tick;
        }
    }
    ImGui::EndChild();
}

/// Optional ImGui demo window drawing (enable/disable-able).
void imgui_demo_window_content(TXP::Input::Input_handler const& input_handler)
{
    // ImGui demo window.
    static size_t s_prev_key_d_event_tick{ 0 };
    if (auto ks = input_handler.get_keyboard_key_state(BT_KEY_D);
        ks.last_event_tick > s_prev_key_d_event_tick)
    {
        if (ks.pressed && ks.modbits.has_control() && ks.modbits.has_alt() &&
            ks.modbits.has_shift())
        {
            s_show_demo_window = !s_show_demo_window;
        }
        s_prev_key_d_event_tick = ks.last_event_tick;
    }
    if (s_show_demo_window)
    {
        ImGui::ShowDemoWindow();
    }
}

}  // namespace


void editor_content::set_imgui_build_contents_callback(std::function<void()>&& callback)
{
    s_imgui_build_contents_callback_fn = callback;
}

void editor_content::set_imguizmo_enabled(bool flag)
{
    ImGuizmo::Enable(flag);
}

void editor_content::add_to_imguizmo_manipulate(mat4 transform,
                                                std::function<void(mat4 const)>&& changed_callback)
{
    Manipulate_transform nt{ .manipulated_callback = std::move(changed_callback) };
    glm_mat4_copy(transform, nt.transform);

    s_manipulate_transform_list.emplace_back(std::move(nt));
}

void editor_content::build_content(TXP::Renderer_settings& settings,
                                   TXP::Input::Input_handler const& input_handler,
                                   Render_view_image_content const& render_view_image_content,
                                   std::function<void(bool)> const& lock_cursor_fn,
                                   Camera_internal& camera,
                                   Information_hook_struct const& info_hook_struct,
                                   std::vector<Render_view_size>& out_rend_view_sizes)
{
    static bool s_play_flag{ info_hook_struct.get_play_flag_fn() };
    bool focus_main_viewport{ false };

    enum Editor_mode
    {
        EDITOR_MODE_LEVEL,
        EDITOR_MODE_ANIM_FRAME_ACTION,
    };
    static Editor_mode s_editor_mode{ EDITOR_MODE_LEVEL };

    // Main menu bar.
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Simulation"))
        {
            ImGui::BeginDisabled(s_editor_mode != EDITOR_MODE_LEVEL);

            if (ImGui::MenuItem("Simulation playing", nullptr, s_play_flag))
            {
                s_play_flag = !s_play_flag;
                info_hook_struct.set_play_flag_fn(s_play_flag);

                // Focus onto main viewport when starting simulation.
                if (s_play_flag)
                    focus_main_viewport = true;
            }

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Editor Mode"))
        {
            auto prev_mode{ s_editor_mode };

            ImGui::BeginDisabled(s_play_flag);

            if (ImGui::MenuItem("Level Editor", "", s_editor_mode == EDITOR_MODE_LEVEL))
            {
                s_editor_mode = EDITOR_MODE_LEVEL;
            }
            if (ImGui::MenuItem("Anim Frame Action Editor",
                                "",
                                s_editor_mode == EDITOR_MODE_ANIM_FRAME_ACTION))
            {
                s_editor_mode = EDITOR_MODE_ANIM_FRAME_ACTION;
                info_hook_struct.set_dev_anim_editor_view_fn(true);
            }

            if (s_editor_mode != prev_mode)
            {
                if (prev_mode == EDITOR_MODE_ANIM_FRAME_ACTION)
                {
                    info_hook_struct.set_dev_anim_editor_view_fn(false);
                    anim_frame_action::reset_editor_state();
                }
            }

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::BeginMenu("Add Window"))
            {
                if (ImGui::MenuItem("Scene Editor"))
                {  // Adds an editor window.
                    size_t new_id{ 0 };

                    bool is_id_unique{ false };
                    while (!is_id_unique)
                    {
                        auto new_id_as_str{ std::to_string(new_id) };
                        bool found{ false };
                        for (auto const& id : settings.open_scene_view_ids)
                            if (id == new_id_as_str)
                            {
                                found = true;
                                break;
                            }

                        if (!found)
                            is_id_unique = true;
                        else
                            new_id++;
                    }

                    settings.open_scene_view_ids.emplace_back(std::to_string(new_id));
                }
                ImGui::EndMenu();
        }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Show ImGui demo window", "Ctrl+Alt+Shift+D", s_show_demo_window))
            {
                s_show_demo_window = !s_show_demo_window;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Main dockspace.
    ImGui::DockSpaceOverViewport(0,
                                 ImGui::GetMainViewport(),
                                 0);

    // Just a check.
    assert(s_manipulate_transform_list.empty());

    // Custom imgui information.
    if (!s_imgui_build_contents_callback_fn)
        throw std::runtime_error("ImGui build contents callback not defined!");
    s_imgui_build_contents_callback_fn();

    // Collect size of available content in viewports.
    std::vector<ImVec2> per_viewport_content_sizes;
    per_viewport_content_sizes.reserve(1 + settings.open_scene_view_ids.size());  // +1 for main viewport.

    // Draw main viewport window (only 1).
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Main Viewport"))
    {
        auto window_content_pos = ImGui::GetCursorScreenPos();

        per_viewport_content_sizes.emplace_back(ImGui::GetContentRegionAvail());

    #if TXP_GFX_BACKEND_VULKAN
        auto& img_descriptor{ render_view_image_content.content_image_descriptors.front() };
        ImGui::Image((ImTextureRef)img_descriptor,
                        per_viewport_content_sizes.back());
    #endif // TXP_GFX_BACKEND_VULKAN

        if (s_play_flag)
        {
            imgui_camera_mode_shift_c_ctrl_prompt_overlay(
                input_handler,
                [&camera,
                 &lock_cursor_fn,
                 &focus_main_viewport,
                 imgui_cur_win = ImGui::GetCurrentWindow()]() {
                    if (camera.get_controlling_camera() == 0)
                    {   // Turn off controlled camera.
                        camera.set_controlling_camera(camera.k_controlling_camera_state_none);
                        lock_cursor_fn(false);
                    }
                    else
                    {   // Lock in controlled orbit camera.
                        focus_main_viewport = true;
                    }
                },
                "Press Shift+C to toggle \"Orbit Camera Mode\" on/off.",
                window_content_pos);
        }
    }
    else
    {   // Put in dummy 1x1 view if view is obfuscated or closed.
        per_viewport_content_sizes.emplace_back(ImVec2(1, 1));
    }
    if (focus_main_viewport)
    {
        BT_TRACE("ENTER ORBIT CAM MODE");
        camera.set_controlling_camera(0);
        lock_cursor_fn(true);
        ImGui::FocusWindow(ImGui::GetCurrentWindow());
    }
    ImGui::End();

    // Update mouse input data.
    bool on_rmb_pressed{ false };
    static size_t s_prev_rmb_event_tick{ 0 };
    if (auto rmb = input_handler.get_mouse_button_state(BT_MOUSE_BUTTON_RIGHT);
        rmb.last_event_tick > s_prev_rmb_event_tick)
    {
        on_rmb_pressed = rmb.pressed;
        s_prev_rmb_event_tick = rmb.last_event_tick;
    }

    // Get cam matrices.
    auto const& cam_matrices{ camera.get_calcd_cam_matrices() };

    // Draw all scene editor windows.
    int32_t request_close_scene_editor_window_idx{ -1 };

    static auto const k_create_scene_editor_window_name = [](std::string const& sub_name) {
        return ("Scene Editor##" + sub_name);
    };

    for (size_t i = 0; i < settings.open_scene_view_ids.size(); i++)
    {
        bool is_open{ true };

        ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(k_create_scene_editor_window_name(settings.open_scene_view_ids[i]).c_str(),
                         &is_open))
        {
            auto window_content_pos = ImGui::GetCursorScreenPos();

            auto viewport_size = ImGui::GetContentRegionAvail();
            viewport_size.x = glm_max(viewport_size.x, 1);
            viewport_size.y = glm_max(viewport_size.y, 1);
            per_viewport_content_sizes.emplace_back(viewport_size);

        #if TXP_GFX_BACKEND_VULKAN
            auto& img_descriptor{
                i + 1 < render_view_image_content.content_image_descriptors.size()
                    ? render_view_image_content.content_image_descriptors[i + 1]
                    : render_view_image_content.content_image_descriptors.back()
            };
            ImGui::Image((ImTextureRef)img_descriptor,
                         per_viewport_content_sizes.back());
        #endif // TXP_GFX_BACKEND_VULKAN

            if (ImGui::IsItemHovered() && on_rmb_pressed)
            {
                BT_TRACE("ENTER FLYING CAM MODE");
                camera.set_controlling_camera(i + 1);
                lock_cursor_fn(true);
                ImGui::FocusWindow(ImGui::GetCurrentWindow());
            }

            if (camera.get_controlling_camera() == i + 1)
            {
                imgui_camera_mode_shift_c_ctrl_prompt_overlay(
                    input_handler,
                    [&camera, &lock_cursor_fn]() {
                        camera.set_controlling_camera(camera.k_controlling_camera_state_none);
                        lock_cursor_fn(false);
                    },
                    "Press Shift+C to exit \"Flying Camera Mode\".",
                    window_content_pos);
            }

            // Draw ImGuizmo manipulate transforms.
            ImGuizmo::SetDrawlist();

            auto win_rect{ ImGui::GetCurrentWindowRead()->Rect() };
            ImGuizmo::SetRect(win_rect.GetTL().x,
                              win_rect.GetTL().y,
                              win_rect.GetWidth(),
                              win_rect.GetHeight());

            mat4 manip_trans_cam_view;
            mat4 manip_trans_cam_proj;
            if (!s_manipulate_transform_list.empty())
            {
                auto const& cam_mat{ cam_matrices[i + 1] };

                glm_mat4_copy(const_cast<vec4*>(cam_mat.view), manip_trans_cam_view);

                glm_mat4_copy(const_cast<vec4*>(cam_mat.projection), manip_trans_cam_proj);
                manip_trans_cam_proj[1][1] *= -1;  // Undo neg-Y issue fix.
            }

            for (auto& manip_trans : s_manipulate_transform_list)
            {
                // Draw Imguizmo gizmo.
                mat4 transdebug = GLM_MAT4_IDENTITY_INIT;
                bool manipulated{ false };
                if (ImGuizmo::Manipulate(&manip_trans_cam_view[0][0],
                                         &manip_trans_cam_proj[0][0],
                                         ImGuizmo::UNIVERSAL,
                                         false ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
                                         &manip_trans.transform[0][0]))
                {
                    manip_trans.manipulated_callback(manip_trans.transform);
                }
            }
        }
        ImGui::End();

        if (!is_open)
        {   // Close window.
            request_close_scene_editor_window_idx = i;
        }
    }
    if (request_close_scene_editor_window_idx >= 0)
    {   // Perform real close window.
        size_t i = request_close_scene_editor_window_idx;

        auto window_name = k_create_scene_editor_window_name(settings.open_scene_view_ids[i]);
        ImGui::ClearWindowSettings(window_name.c_str());

        settings.open_scene_view_ids.erase(settings.open_scene_view_ids.begin() + i);
    }

    // Clear used manipulate transform list.
    s_manipulate_transform_list.clear();

    // Render performance timers.
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance timers"))
    {
        for (uint32_t i = 0; i < NUM_PERF_TIME_TYPES; i++)
        {
            auto perf_time_type{ static_cast<Performance_time_type>(i) };
            if (info_hook_struct.perf_time_map.find(perf_time_type) ==
                info_hook_struct.perf_time_map.end())
                continue;

            enum Perf_quality
            {
                PERFQ_ABOVE_120fps = 0,
                PERFQ_ABOVE_60fps,
                PERFQ_ABOVE_30fps,
                PERFQ_ABOVE_15fps,
                PERFQ_GARBAGE,

                NUM_PERF_QLYS
            };
            static std::array<ImU32, NUM_PERF_QLYS> const k_perf_quality_color_map{
                ImGui::GetColorU32(ImVec4(0, 1, 0, 1)),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 1)),
                ImGui::GetColorU32(ImVec4(1, 1, 0, 1)),
                ImGui::GetColorU32(ImVec4(1, 0, 1, 1)),
                ImGui::GetColorU32(ImVec4(1, 0, 0, 1)),
            };
            static auto const k_calc_perf_quality = [](float_t fps) {
                Perf_quality quality = PERFQ_GARBAGE;
                if (fps >= 120)
                    quality = PERFQ_ABOVE_120fps;
                else if (fps >= 60)
                    quality = PERFQ_ABOVE_60fps;
                else if (fps >= 30)
                    quality = PERFQ_ABOVE_30fps;
                else if (fps >= 15)
                    quality = PERFQ_ABOVE_15fps;
                return quality;
            };

            auto perf_samples = info_hook_struct.perf_time_map.at(perf_time_type).get_samples();
            ImGui::PushID(&perf_samples);

            float_t highest_sample = 0;
            for (auto s : perf_samples)
                highest_sample = glm_max(highest_sample, s);

            float_t latest_sample = perf_samples.back();
            float_t ms = latest_sample * 1000.0f;
            float_t fps = 1.0f / latest_sample;

            constexpr float_t k_ms_max_scale{ 500 };

            // Draw stats.
            ImGui::Text("PERF TIMER: %s", k_performance_time_type_labels[perf_time_type]);
            ImGui::Text(" %.2f ms (%.0f FPS)", ms, fps);

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                  k_perf_quality_color_map[k_calc_perf_quality(fps)]);
            ImGui::ProgressBar(ms / k_ms_max_scale, ImVec2(-FLT_MIN, 2), "");
            ImGui::PopStyleColor();

            float_t cur_ms_max_scale = (10 / 1000.0f);
            while (highest_sample > cur_ms_max_scale)
                cur_ms_max_scale *= 2;
            while (highest_sample <= cur_ms_max_scale * 0.5f)
                cur_ms_max_scale *= 0.5f;

            float_t content_width{ ImGui::GetContentRegionAvail().x };

            size_t clamped_perf_samples_size = glm_min(perf_samples.size(), content_width);
            size_t right_align_perf_samples_idx_offset =
                glm_max(0, perf_samples.size() - 1 - content_width);

            ImGui::PlotLines(("##plotlines" + std::to_string(i)).c_str(),
                             perf_samples.data() + right_align_perf_samples_idx_offset,
                             clamped_perf_samples_size,
                             0,
                             nullptr,
                             0,
                             glm_min(cur_ms_max_scale, k_ms_max_scale),
                             ImVec2(content_width, 32));

            ImGui::PopID();
        }
    }
    ImGui::End();

    // .
    imgui_demo_window_content(input_handler);

    // Report back render view sizes.
    out_rend_view_sizes.reserve(per_viewport_content_sizes.size());
    for (auto const& per_viewport_content_size : per_viewport_content_sizes)
    {
        out_rend_view_sizes.emplace_back(
            Render_view_size{ .width = static_cast<int32_t>(per_viewport_content_size.x),
                              .height = static_cast<int32_t>(per_viewport_content_size.y) });
    }
}

}  // namespace TXP
