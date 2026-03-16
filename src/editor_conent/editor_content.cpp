#include "editor_content.h"

#include "btlogger.h"
#include "btuuid.h"
#include "imgui.h"
#include "input_handler/input_handler.h"
#include "input_handler/input_key_codes.h"

#include <cmath>
#include <vector>


namespace TXP
{
namespace
{

bool s_show_demo_window{ false };

size_t s_num_scene_editor_windows{ 1 };
std::vector<BT::UUID> s_active_scene_editor_window_uuids;

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


void editor_content::build_content(TXP::Input::Input_handler const& input_handler)
{
    // Main menu bar.
    if (ImGui::BeginMainMenuBar())
    {
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

    // Prompt for cursor to get unlocked.
    constexpr float_t k_padding{ 10.0f };
    ImGuiViewport const& viewport{ *ImGui::GetMainViewport() };
    ImGui::SetNextWindowPos(ImVec2(viewport.WorkPos.x + k_padding, viewport.WorkPos.y + k_padding),
                            ImGuiCond_Always,
                            ImVec2(0, 0));

    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("##Overlay Window for Prompt to Unlock Cursor",
                     nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Press Shift+U to exit \"Flying Camera Mode\".");
    }
    ImGui::End();

    // Draw main viewport window (only 1).
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Main Viewport"))
    {
        ImGui::Text("@TODO: put view image here!!");
        ImGui::End();
    }

    // Update mouse input data.
    bool on_rmb_pressed{ false };
    static size_t s_prev_rmb_event_tick{ 0 };
    if (auto rmb = input_handler.get_mouse_button_state(BT_MOUSE_BUTTON_RIGHT);
        rmb.last_event_tick > s_prev_rmb_event_tick)
    {
        on_rmb_pressed = rmb.pressed;
        s_prev_rmb_event_tick = rmb.last_event_tick;
    }

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
            ImGui::Text("@TODO: put view image here!!");

            if (ImGui::IsItemHovered() && on_rmb_pressed)
            {
                BT_TRACE("ENTER FLYING CAM MODE");
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

    // .
    imgui_demo_window_content(input_handler);

    // if (!imgui_build_contents_callback)  @TODO
    //     throw std::runtime_error("ImGui build contents callback not defined!");
    // imgui_build_contents_callback();
}

}  // namespace TXP
