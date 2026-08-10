#include "editor_content.h"

#if TXP_GFX_BACKEND_VULKAN
#include "backends/imgui_impl_vulkan.h"
#endif // TXP_GFX_BACKEND_VULKAN

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

#include <cmath>
#include <functional>
#include <vector>


namespace TXP
{
namespace
{

bool s_show_demo_window{ false };

size_t s_num_scene_editor_windows{ 1 };
std::vector<BT::UUID> s_active_scene_editor_window_uuids;

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

void editor_content::build_content(TXP::Input::Input_handler const& input_handler,
                                   Render_view_image_content const& render_view_image_content,
                                   std::function<void(bool)> const& lock_cursor_fn,
                                   Camera_internal& camera,
                                   Information_hook_struct const& info_hook_struct,
                                   std::vector<Render_view_size>& out_rend_view_sizes)
{
    static bool s_play_flag{ info_hook_struct.get_play_flag_fn() };
    bool focus_main_viewport{ false };

    // Main menu bar.
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Simulation"))
        {
            if (ImGui::MenuItem("Simulation playing", nullptr, s_play_flag))
            {
                s_play_flag = !s_play_flag;
                info_hook_struct.set_play_flag_fn(s_play_flag);

                // Focus onto main viewport when starting simulation.
                if (s_play_flag)
                    focus_main_viewport = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::MenuItem("Scene Editor"))
            {   // Adds an editor window.
                s_num_scene_editor_windows++;
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
    per_viewport_content_sizes.reserve(1 + s_num_scene_editor_windows);  // +1 for main viewport.

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
    for (size_t i = 0; i < s_num_scene_editor_windows; i++)
    {
        if (i >= s_active_scene_editor_window_uuids.size())
        {   // Register new window id.
            s_active_scene_editor_window_uuids.emplace_back(BT::UUID_helper::generate_uuid());
        }

        bool is_open{ true };
        ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(("Scene Editor##" +
                          BT::UUID_helper::to_pretty_repr(s_active_scene_editor_window_uuids[i]))
                             .c_str(),
                         &is_open))
        {
            auto window_content_pos = ImGui::GetCursorScreenPos();

            per_viewport_content_sizes.emplace_back(ImGui::GetContentRegionAvail());

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

            auto win_rect{ ImGui::GetCurrentWindowRead()->Rect() };  // @TODO: needed???
            ImGuizmo::SetRect(win_rect.GetTL().x,
                              win_rect.GetTL().y,
                              win_rect.GetWidth(),
                              win_rect.GetHeight());

            auto const& cam_mat{ cam_matrices[i + 1] };

            for (auto& manip_trans : s_manipulate_transform_list)
            {
                // Draw Imguizmo gizmo.
                mat4 transdebug = GLM_MAT4_IDENTITY_INIT;
                bool manipulated{ false };
                if (ImGuizmo::Manipulate(&cam_mat.view[0][0],
                                         &cam_mat.projection[0][0],
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
            s_active_scene_editor_window_uuids.erase(s_active_scene_editor_window_uuids.begin() +
                                                     i);
            s_num_scene_editor_windows--;
        }
    }

    // Clear used manipulate transform list.
    s_manipulate_transform_list.clear();

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
