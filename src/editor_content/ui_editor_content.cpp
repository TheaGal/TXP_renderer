#include "ui_editor_content.h"

#include "btlogger.h"
#include "btservice_finder.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "txp_renderer/ui/ui_state.h"
#include "ui/ui_types.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

void UI_editor::force_ui_redraw()
{
    m_ui_ref.invalidate_draw_cache();
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
    m_editing_canvas_element_ptr = nullptr;

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

void UI_editor::set_selected_editing_elem(UI::UI_element* elem)
{
    m_editing_canvas_element_ptr = elem;
}

UI::UI_element* UI_editor::get_selected_editing_elem()
{
    return m_editing_canvas_element_ptr;
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


namespace
{

struct UI_hierarchy_node
{
    UI::UI_element* node;
    std::vector<UI_hierarchy_node*> children;
};

/// Gets appropriate node flags depending on the modifiers.  @COPYPASTA
constexpr ImGuiTreeNodeFlags get_tree_node_flags(bool is_selected, bool is_leaf_node)
{   // Base flags.
    ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
                              ImGuiTreeNodeFlags_DrawLinesToNodes |
                              ImGuiTreeNodeFlags_OpenOnDoubleClick |
                              ImGuiTreeNodeFlags_OpenOnArrow |
                              ImGuiTreeNodeFlags_DefaultOpen };

    if (is_selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    if (is_leaf_node)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    return flags;
}

void render_ui_hierarchy_level_recurse(UI_hierarchy_node* node)
{
    bool const is_leaf_node{ node->children.empty() };

    bool const tree_node_open = ImGui::TreeNodeEx(
        (node->node->name + "##ui_hierarchy_node").c_str(),
        get_tree_node_flags(s_ui_editor->get_selected_editing_elem() == node->node, is_leaf_node));

    if (ImGui::IsItemClicked())
        s_ui_editor->set_selected_editing_elem(node->node);

    if (tree_node_open)
    {
        for (auto* node_child : node->children)
            render_ui_hierarchy_level_recurse(node_child);

        if (!is_leaf_node)
            ImGui::TreePop();
    }
}

// @COPYPASTA
void render_deselect_ui_elem_field()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
    ImGui::ButtonEx("##deselect_ui_elem_field_hidden_button",
                    ImVec2(ImGui::GetContentRegionAvail().x,
                        std::max(24.0f, ImGui::GetContentRegionAvail().y)),
                    ImGuiButtonFlags_NoNavFocus);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (ImGui::IsItemClicked())
        s_ui_editor->set_selected_editing_elem(nullptr);
}

void render_ui_hierarchy()
{
    // Create hierarchy elems structure.
    std::vector<UI_hierarchy_node> all_nodes;
    all_nodes.reserve(s_ui_editor->get_editing_elems().size());

    for (auto* elem : s_ui_editor->get_editing_elems())
    {
        all_nodes.emplace_back(elem, std::vector<UI_hierarchy_node*>{});
    }

    std::vector<UI_hierarchy_node*> root_nodes;

    for (auto& hier_node : all_nodes)
    {
        UI::UI_element* parent_node{ hier_node.node->parent };
        if (parent_node == nullptr)
        {
            root_nodes.emplace_back(&hier_node);
            continue;
        }

        for (auto& search_node : all_nodes)
            if (search_node.node == parent_node)
            {
                search_node.children.emplace_back(&hier_node);
                break;
            }
    }

    // Render out hierarchy.
    for (auto* node : root_nodes)
    {
        render_ui_hierarchy_level_recurse(node);
    }

    render_deselect_ui_elem_field();
}

} // namespace


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

            render_ui_hierarchy();
        }
    }
    ImGui::End();

    // UI element editor.
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("UI element editor");
    if (s_ui_editor->get_editing_canvas_idx() < 0 ||
        s_ui_editor->get_selected_editing_elem() == nullptr)
    {
        ImGui::Text("%s", "Select a UI element to edit.");
    }
    else
    {
        // Edit the properties of the UI element.
        static size_t const s_id_gen{ 0xdefec8 };
        ImGui::PushID(reinterpret_cast<void*>(s_id_gen));

        UI::UI_element* editing_elem{ s_ui_editor->get_selected_editing_elem() };

        bool edited{ false };

        edited |= ImGui::InputText("name", &editing_elem->name);
        /*edited |=*/ ImGui::Text("parent: %p", editing_elem->parent);
        if (ImGui::TreeNodeEx("transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            edited |= ImGui::DragFloat("x", &editing_elem->transform.x);
            edited |= ImGui::DragFloat("y", &editing_elem->transform.y);
            edited |= ImGui::DragFloat("w", &editing_elem->transform.w);
            edited |= ImGui::DragFloat("h", &editing_elem->transform.h);
            edited |= ImGui::DragFloat("rot", &editing_elem->transform.rot, 2 * M_PI / 360);

            if (ImGui::TreeNodeEx("anchor", ImGuiTreeNodeFlags_DefaultOpen))
            {
                edited |= ImGui::DragFloat("x", &editing_elem->transform.anchor.x, 0.01);
                edited |= ImGui::DragFloat("y", &editing_elem->transform.anchor.y, 0.01);

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
        edited |= ImGui::DragFloat("opacity", &editing_elem->opacity, 0.01, 0, 1);

        {
            // Dropdown for texture indexes.
            auto tex_name_w_idx_list{ gfx_helper::fetch_texture_name_w_idx_list() };

            std::unordered_map<uint32_t, int32_t> gpu_tex_idx_to_texture_name_list_idx_map;
            gpu_tex_idx_to_texture_name_list_idx_map.reserve(tex_name_w_idx_list.size());

            std::unordered_map<int32_t, uint32_t> texture_name_list_idx_to_gpu_tex_idx_map;
            texture_name_list_idx_to_gpu_tex_idx_map.reserve(tex_name_w_idx_list.size());

            std::vector<char const*> texture_names;
            texture_names.reserve(tex_name_w_idx_list.size());

            for (auto const& [tex_name, tex_idx] : tex_name_w_idx_list)
            {
                gpu_tex_idx_to_texture_name_list_idx_map[tex_idx] = texture_names.size();
                texture_name_list_idx_to_gpu_tex_idx_map[texture_names.size()] = tex_idx;
                texture_names.emplace_back(tex_name.c_str());
            }

            int32_t cur_tex_idx{ gpu_tex_idx_to_texture_name_list_idx_map.at(
                editing_elem->texture_idx) };
            if (ImGui::Combo("texture", &cur_tex_idx, texture_names.data(), texture_names.size()))
            {
                editing_elem->texture_idx =
                    texture_name_list_idx_to_gpu_tex_idx_map.at(cur_tex_idx);
                edited |= true;
            }
        }

        edited |= ImGui::ColorEdit3("color_tint", editing_elem->color_tint);
        edited |= ImGui::Checkbox("is_nine_slice", &editing_elem->is_nine_slice);

        ImGui::PopID();

        if (edited)
            s_ui_editor->force_ui_redraw();
    }
    ImGui::End();
}
