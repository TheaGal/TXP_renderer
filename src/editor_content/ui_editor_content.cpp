#include "ui_editor_content.h"

#include "btlogger.h"
#include "btservice_finder.h"
#include "imgui.h"
#include "txp_renderer/ui/ui_state.h"
#include "ui/ui_types.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace TXP;

std::vector<std::string> s_avail_ui_files;
std::vector<size_t> s_cur_loaded_ui_files;

editor_content::ui_editor::UI_editor* s_ui_editor{ nullptr };

} // namespace


namespace TXP
{
namespace editor_content
{
namespace ui_editor
{

// class UI_editor.
UI_editor::UI_editor(UI_state& ui_ref)
    : m_ui_ref(ui_ref)
{
    // Hijack UI state.
    m_pre_hijack_state.loaded_canvases = m_ui_ref.m_loaded_canvases;
    m_pre_hijack_state.loaded_persistent_canvases = m_ui_ref.m_loaded_persistent_canvases;

    for (size_t i = 0; i < m_ui_ref.m_loaded_canvases.size(); i++)
    {
        m_ui_ref.unload_front_canvas();
    }
    for (auto const& canvas : m_ui_ref.m_loaded_persistent_canvases)
    {
        m_ui_ref.unload_persistent_canvas(canvas);
    }
}

UI_editor::~UI_editor()
{
    // Unload hijacked state.
    for (size_t i = 0; i < m_ui_ref.m_loaded_canvases.size(); i++)
    {
        m_ui_ref.unload_front_canvas();
    }
    for (auto const& canvas : m_ui_ref.m_loaded_persistent_canvases)
    {
        m_ui_ref.unload_persistent_canvas(canvas);
    }

    // Restore hijacked UI.
    for (auto const& canvas : m_pre_hijack_state.loaded_canvases)
    {
        m_ui_ref.load_canvas_to_front(canvas);
    }
    for (auto const& canvas : m_pre_hijack_state.loaded_persistent_canvases)
    {
        m_ui_ref.load_persistent_canvas(canvas);
    }
}

void UI_editor::load_canvas_to_edit(std::string const& canvas_name)
{
    m_ui_ref.canvas(canvas_name);
    m_ui_ref.load_persistent_canvas(canvas_name);
    m_ui_ref.tick();

    // Assign a loaded index.
    size_t i = 0;
    for (auto const& fname : s_avail_ui_files)
    {
        if (fname == canvas_name)
        {   // Found!
            s_cur_loaded_ui_files.emplace_back(i);
            break;
        }
        i++;
    }
    if (i == s_avail_ui_files.size())
        throw std::runtime_error("Could not find canvas fname to load.");
}

void UI_editor::unload_editing_canvas(std::string const& canvas_name)
{
    m_ui_ref.unload_persistent_canvas(canvas_name);
    m_ui_ref.tick();

    // Assign a loaded index.
    size_t i = 0;
    for (auto const& fname : s_avail_ui_files)
    {
        if (fname == canvas_name)
        {   // Found!
            s_cur_loaded_ui_files.erase(
                std::remove(s_cur_loaded_ui_files.begin(), s_cur_loaded_ui_files.end(), i),
                s_cur_loaded_ui_files.end());
            break;
        }
        i++;
    }
    if (i == s_avail_ui_files.size())
        throw std::runtime_error("Could not find canvas fname to unload.");
}

int32_t UI_editor::get_editing_canvas_idx() const
{
    return m_editing_canvas_idx;
}

void UI_editor::build_editing_elems_for_canvas_idx(int32_t const canvas_idx)
{
    m_editing_elems.clear();

    m_editing_canvas_idx = canvas_idx;

    if (canvas_idx < 0)
        return;

    // Load in canvas if not loaded.
    if (std::find(s_cur_loaded_ui_files.begin(), s_cur_loaded_ui_files.end(), canvas_idx) ==
        s_cur_loaded_ui_files.end())
    {
        load_canvas_to_edit(s_avail_ui_files[canvas_idx]);
    }

    // Build the editing elems.
    auto& my_elems{ reinterpret_cast<std::vector<UI::UI_element>&>(
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elems) };

    for (auto& elem : my_elems)
        m_editing_elems.emplace_back(&elem);
}

std::vector<UI::UI_element*>& UI_editor::get_editing_elems()
{
    return m_editing_elems;
}

} // namespace ui_editor
} // namespace editor_content
} // namespace TXP


void TXP::editor_content::ui_editor::open_editor()
{
    s_ui_editor = new UI_editor(BT::service_finder::find_service<UI_state>());

    s_avail_ui_files = fetch_all_ui_files();
    assert(!s_avail_ui_files.empty());

    for (auto const& fname : s_avail_ui_files)
        BT_TRACEF("Found UI file: %s", fname.c_str());

    assert(s_cur_loaded_ui_files.empty());
}

void TXP::editor_content::ui_editor::close_editor()
{
    s_cur_loaded_ui_files.clear();
    delete s_ui_editor;
}

void TXP::editor_content::ui_editor::ui_editor_content()
{
    assert(!s_avail_ui_files.empty());

    // UI file viewer.
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("View UI files");
    {
        size_t i = 0;
        for (auto const& fname : s_avail_ui_files)
        {
            bool is_loaded{ std::find(s_cur_loaded_ui_files.begin(),
                                      s_cur_loaded_ui_files.end(),
                                      i) != s_cur_loaded_ui_files.end() };

            int32_t const loaded_level{ [ui_file_idx = i]() -> int32_t {
                for (size_t i = 0; i < s_cur_loaded_ui_files.size(); i++)
                    if (s_cur_loaded_ui_files[i] == ui_file_idx)
                    {
                        return (s_cur_loaded_ui_files.size() - i);
                    }
                return -1;
            }() };

            ImGui::BeginDisabled(s_ui_editor->get_editing_canvas_idx() == i);

            if (ImGui::Checkbox((fname + "##load_ui_file_btn").c_str(), &is_loaded))
            {
                if (is_loaded)
                    s_ui_editor->load_canvas_to_edit(fname);
                else
                    s_ui_editor->unload_editing_canvas(fname);
            }
            if (loaded_level >= 0)
            {
                ImGui::SameLine();
                ImGui::Text("(%d)", loaded_level);
            }

            ImGui::EndDisabled();

            i++;
        }
    }
    ImGui::End();

    // UI canvas editor.
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("UI canvas editor");
    {
        // Build cstr list of fnames.
        std::vector<char const*> ui_files_as_cstr;
        ui_files_as_cstr.reserve(s_avail_ui_files.size());
        for (auto const& fname : s_avail_ui_files)
            ui_files_as_cstr.emplace_back(fname.c_str());

        // Load a ui file for editing.
        int32_t editing_canvas_idx{ s_ui_editor->get_editing_canvas_idx() };
        if (ImGui::Combo("Editing UI File##ui_canvas_editor_dropdown",
                         &editing_canvas_idx,
                         ui_files_as_cstr.data(),
                         ui_files_as_cstr.size()))
        {
            s_ui_editor->build_editing_elems_for_canvas_idx(editing_canvas_idx);
        }
        if (editing_canvas_idx >= 0)
        {
            if (ImGui::Button("Save"))
            {
                assert(false);
            }

            ImGui::SameLine();
            if (ImGui::Button("Discard"))
            {
                assert(false);
            }

            ImGui::SameLine();
            if (ImGui::Button("Close"))
            {
                s_ui_editor->build_editing_elems_for_canvas_idx(-1);
                editing_canvas_idx = -1;
            }
        }

        ImGui::Separator();

        // Editing section.
        if (editing_canvas_idx < 0)
            ImGui::Text("%s", "Load a new UI file from the dropdown to edit it.");
        else
        {
            if (ImGui::Button("Create new elem.."))
                ImGui::OpenPopup("create_new_ui_elem_popup");
            if (ImGui::BeginPopup("create_new_ui_elem_popup"))
            {
                ImGui::Text("@TODO");

                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::Text("%zu elements", s_ui_editor->get_editing_elems().size());

            for (auto* elem : s_ui_editor->get_editing_elems())
            {
                ImGui::Text("%s (%p)", elem->name.c_str(), elem);
            }
        }
    }
    ImGui::End();
}
