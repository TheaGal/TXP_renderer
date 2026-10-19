#include "ui_editor_content.h"

#include "btlogger.h"
#include "btservice_finder.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "txp_renderer/ui/ui_state.h"
#include "ui/ui_element_list_container_impl.h"
#include "ui/ui_types.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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

void UI_editor::load_canvas_to_view(std::string const& canvas_name)
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

void UI_editor::unload_viewing_canvas(std::string const& canvas_name)
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

void UI_editor::build_editing_elems_for_canvas_idx(int32_t const canvas_idx,
                                                   bool const force_reload)
{
    m_editing_elems.clear();

    m_editing_canvas_idx = canvas_idx;
    m_editing_canvas_element_ptr = nullptr;
    make_not_dirty_also_known_as_clean();

    if (canvas_idx < 0)
        return;

    // Load in canvas if not loaded.
    if (std::find(s_cur_loaded_ui_files.begin(), s_cur_loaded_ui_files.end(), canvas_idx) ==
        s_cur_loaded_ui_files.end())
    {
        load_canvas_to_view(s_avail_ui_files[canvas_idx]);
    }
    else if (force_reload)
    {
        unload_viewing_canvas(s_avail_ui_files[canvas_idx]);
        load_canvas_to_view(s_avail_ui_files[canvas_idx]);
    }

    // Build the editing elems.
    auto& my_elems{
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elem_container->elems
    };

    for (auto& elem : my_elems)
        m_editing_elems.emplace_back(&elem);
}


namespace
{

void perform_breaking_operation_safe(std::vector<UI::UI_element>& my_elems,
                                     std::vector<UI::UI_element*>& my_editing_elems,
                                     std::function<void()>&& operation)
{
    // Build pointer -> idx map to recover from emplace breaking memory addresses.
    std::unordered_map<void*, size_t> ptr_to_idx_map;
    for (auto const& elem : my_elems)
    {
        for (size_t i = 0; i < my_elems.size(); i++)
            if (elem.parent == nullptr)
            {
                break;
            }
            else if (elem.parent == &my_elems[i])
            {
                ptr_to_idx_map[elem.parent] = i;
                break;
            }
    }

    // Do operation!!
    operation();

    // Rebuild parent pointers.
    for (auto& elem : my_elems)
        if (elem.parent != nullptr && ptr_to_idx_map.find(elem.parent) != ptr_to_idx_map.end())
        {
            elem.parent = &my_elems[ptr_to_idx_map.at(elem.parent)];
        }

    // Rebuild editing elems pointer list.
    my_editing_elems.clear();
    for (auto& elem : my_elems)
        my_editing_elems.emplace_back(&elem);
}

} // namespace


void UI_editor::add_elem_to_editing_canvas(UI::UI_element&& elem)
{
    auto& my_elems{
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elem_container->elems
    };

    perform_breaking_operation_safe(my_elems, m_editing_elems, [&my_elems, &elem]() {
        my_elems.emplace_back(std::move(elem));
    });

    m_editing_canvas_element_ptr = m_editing_elems.back();
}

void UI_editor::save_editing_canvas() const
{
    m_ui_ref.save_persistent_canvas_to_disk(s_avail_ui_files[m_editing_canvas_idx]);
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

void UI_editor::delete_selected_editing_elem()
{
    auto& my_elems{
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elem_container->elems
    };

    // Collect all children or self of deleted elem.
    std::vector<size_t> elem_idxs_to_delete;
    for (size_t i = 0; i < my_elems.size(); i++)
    {
        UI::UI_element const* cursor{ &my_elems[i] };
        do
        {
            if (cursor == m_editing_canvas_element_ptr)
            {
                elem_idxs_to_delete.emplace_back(i);
                break;
            }
            cursor = cursor->parent;
        } while (cursor != nullptr);
    }

    std::ranges::sort(elem_idxs_to_delete, std::ranges::greater());  // sort desc

    perform_breaking_operation_safe(my_elems, m_editing_elems, [&my_elems, &elem_idxs_to_delete]() {
        for (size_t delete_idx : elem_idxs_to_delete)
            my_elems.erase(my_elems.begin() + delete_idx);
    });

    m_editing_canvas_element_ptr = nullptr;
}

void UI_editor::move_selected_editing_elem(int32_t offset)
{
    auto& my_elems{
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elem_container->elems
    };

    auto const* my_parent{ m_editing_canvas_element_ptr->parent };

    std::vector<UI::UI_element*> elems_with_same_parent;
    for (auto& elem : my_elems)
        if (elem.parent == my_parent)
            elems_with_same_parent.emplace_back(&elem);

    size_t swap_a;
    size_t swap_b;

    for (size_t i = 0; i < elems_with_same_parent.size(); i++)
        if (elems_with_same_parent[i] == m_editing_canvas_element_ptr)
        {
            swap_a = i;
            swap_b = swap_a + offset;

            if (swap_b >= elems_with_same_parent.size())  // loops so no eg numbers
            {
                // Invalid swap position. Silently quit.
                return;
            }

            break;
        }

    // Perform swap (Assume no reallocations!!)
    std::swap(*elems_with_same_parent[swap_a], *elems_with_same_parent[swap_b]);

    // Rebuild pointers.
    for (auto& elem : my_elems)
    {
        if (elem.parent == elems_with_same_parent[swap_a])
            elem.parent = elems_with_same_parent[swap_b];
        else if (elem.parent == elems_with_same_parent[swap_b])
            elem.parent = elems_with_same_parent[swap_a];
    }

    m_editing_canvas_element_ptr = elems_with_same_parent[swap_b];
}

void UI_editor::sort_editing_canvas_elems()
{
    auto& my_elems{
        m_ui_ref.canvas(s_avail_ui_files[m_editing_canvas_idx]).m_elem_container->elems
    };

    // Do a quick circular dependency check.
    for (auto const& elem : my_elems)
    {
        UI::UI_element const* ptr = elem.parent;
        bool found_end{ false };

        for (uint32_t i = 0; i < 10000; i++)
            if (ptr == nullptr)
            {
                found_end = true;
                break;
            }
            else
                ptr = ptr->parent;
        
        if (!found_end)
            throw std::runtime_error("Circular dependency oh no!!!");
    }

    // Find relationships.
    using Elem_w_idx_t = std::pair<UI::UI_element const*, size_t>;

    std::vector<Elem_w_idx_t> elems_in_order;

    std::vector<Elem_w_idx_t> current_depth;
    current_depth.emplace_back(nullptr, 0);

    while (!current_depth.empty())
    {
        auto& [looking_for_parent, cursor]{ current_depth.back() };

        bool performed_emplace{ false };

        for (; cursor < my_elems.size(); cursor++)
        {
            // Look for children.
            auto& this_elem{ my_elems[cursor] };
            if (looking_for_parent == this_elem.parent)
            {
                elems_in_order.emplace_back(&this_elem, cursor);

                cursor++;  // do before the `current_depth` emplace
                current_depth.emplace_back(&this_elem, 0);

                performed_emplace = true;
                break;
            }
        }

        if (!performed_emplace && cursor >= my_elems.size())
        {
            current_depth.erase(current_depth.begin() + current_depth.size() - 1);
        }
    }

    assert(elems_in_order.size() == my_elems.size());

    // Make conversion map.
    std::unordered_map<UI::UI_element const*, size_t> elem_ptr_to_idx_map;
    std::unordered_map<size_t, size_t> old_to_new_idx_map;
    for (size_t i = 0; i < elems_in_order.size(); i++)
    {
        elem_ptr_to_idx_map[elems_in_order[i].first] = elems_in_order[i].second;
        old_to_new_idx_map[elems_in_order[i].second] = i;
    }

    // Is the remap even necessary?
    bool should_do_remap{ false };
    for (auto const [old_idx, new_idx] : old_to_new_idx_map)
        if (old_idx != new_idx)
        {
            should_do_remap = true;
            break;
        }

    if (!should_do_remap)
        return;

    mark_dirty();

    // Do remap.
    std::vector<UI::UI_element> elems_copy = my_elems;
    for (size_t i = 0; i < my_elems.size(); i++)
    {
        my_elems[old_to_new_idx_map.at(i)] = elems_copy[i];
    }

    // Fix broken parent pointers.
    for (auto& elem : my_elems)
    {
        if (elem.parent != nullptr)
            elem.parent = &my_elems[old_to_new_idx_map.at(elem_ptr_to_idx_map.at(elem.parent))];
    }
}

void UI_editor::mark_dirty()
{
    m_is_data_dirty = true;
}

bool UI_editor::is_dirty() const
{
    return m_is_data_dirty;
}

void UI_editor::make_not_dirty_also_known_as_clean()
{
    m_is_data_dirty = false;
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
                    s_ui_editor->load_canvas_to_view(fname);
                else
                    s_ui_editor->unload_viewing_canvas(fname);
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
        ImGui::BeginDisabled(s_ui_editor->is_dirty());

        int32_t editing_canvas_idx{ s_ui_editor->get_editing_canvas_idx() };
        if (ImGui::Combo("Editing UI File##ui_canvas_editor_dropdown",
                         &editing_canvas_idx,
                         ui_files_as_cstr.data(),
                         ui_files_as_cstr.size()))
        {
            s_ui_editor->build_editing_elems_for_canvas_idx(editing_canvas_idx, false);
            s_ui_editor->sort_editing_canvas_elems();
        }

        ImGui::EndDisabled();

        if (editing_canvas_idx >= 0)
        {
            ImGui::BeginDisabled(!s_ui_editor->is_dirty());

            if (ImGui::Button("Save"))
            {
                s_ui_editor->save_editing_canvas();
                s_ui_editor->make_not_dirty_also_known_as_clean();
            }

            ImGui::SameLine();
            if (ImGui::Button("Discard"))
            {
                s_ui_editor->build_editing_elems_for_canvas_idx(editing_canvas_idx, true);
            }

            ImGui::EndDisabled();

            ImGui::BeginDisabled(s_ui_editor->is_dirty());

            ImGui::SameLine();
            if (ImGui::Button("Close"))
            {
                s_ui_editor->build_editing_elems_for_canvas_idx(-1, false);
                editing_canvas_idx = -1;
            }

            ImGui::EndDisabled();
        }

        ImGui::Separator();

        // Editing section.
        if (editing_canvas_idx < 0)
            ImGui::Text("%s", "Load a new UI file from the dropdown to edit it.");
        else
        {
            static auto const k_get_non_colliding_name =
                [](std::vector<UI::UI_element*> const& editing_elems,
                   std::string const& base_name) {
                    std::string new_name = base_name;
                    int32_t suffix_counter{ 0 };

                    bool collision;
                    do
                    {
                        collision = false;

                        std::string try_name =
                            new_name +
                            (suffix_counter > 0 ? " (" + std::to_string(suffix_counter) + ")" : "");
                        for (auto const* elem : editing_elems)
                            if (elem->name == try_name)
                            {
                                collision = true;
                                break;
                            }

                        if (!collision)
                            new_name = try_name;
                        else
                            suffix_counter++;
                    } while (collision);

                    return new_name;
                };

            if (ImGui::Button("Create new elem"))
            {
                s_ui_editor->add_elem_to_editing_canvas(UI::UI_element{
                    .name = k_get_non_colliding_name(s_ui_editor->get_editing_elems(),
                                                     "New UI element") });

                s_ui_editor->sort_editing_canvas_elems();

                s_ui_editor->force_ui_redraw();
            }

            ImGui::BeginDisabled(s_ui_editor->get_selected_editing_elem() == nullptr);

            ImGui::SameLine();
            if (ImGui::Button("Duplicate elem"))
            {
                // Gather all elems to duplicate.
                std::vector<UI::UI_element const*> elems_to_duplicate;
                elems_to_duplicate.emplace_back(s_ui_editor->get_selected_editing_elem());

                bool keep_going{ true };
                while (keep_going)
                {
                    keep_going = false;

                    for (auto const* elem : s_ui_editor->get_editing_elems())
                    {
                        bool found_parent{ false };
                        bool found_self{ false };

                        for (auto const* elem_to_dupe : elems_to_duplicate)
                        {
                            if (elem == elem_to_dupe)
                            {
                                found_self = true;
                                break;
                            }
                            if (elem->parent == elem_to_dupe)
                            {
                                found_parent = true;
                            }
                        }

                        if (!found_self && found_parent)
                        {
                            keep_going = true;
                            elems_to_duplicate.emplace_back(elem);
                        }
                    }
                }

                // Create new names for elems to dupe.
                std::vector<std::string> dupe_names;
                dupe_names.reserve(elems_to_duplicate.size());

                std::vector<std::string> dupe_parent_names;
                dupe_parent_names.reserve(elems_to_duplicate.size());

                for (auto const* elem_to_dupe : elems_to_duplicate)
                {
                    dupe_names.emplace_back(
                        k_get_non_colliding_name(s_ui_editor->get_editing_elems(),
                                                 elem_to_dupe->name));

                    if (elem_to_dupe->parent == nullptr)
                        dupe_parent_names.emplace_back("");
                    else
                    {
                        for (auto const* editing_elem : s_ui_editor->get_editing_elems())
                            if (editing_elem == elem_to_dupe->parent)
                            {
                                if (dupe_parent_names.empty())
                                    // Keep OG parent name since original dupe's parent isn't
                                    // getting duped.
                                    dupe_parent_names.emplace_back(editing_elem->name);
                                else
                                    // Trust that the non-colliding name will be correct!!!
                                    // (don't worry about this unless the check below fails)
                                    dupe_parent_names.emplace_back(
                                        k_get_non_colliding_name(s_ui_editor->get_editing_elems(),
                                                                 editing_elem->name));
                                break;
                            }
                    }
                }

                // Final name collision check.
                bool must_abort{ false };

                for (size_t i = 0; i < dupe_names.size(); i++)
                    for (size_t j = 0; j < dupe_names.size(); j++)
                        if (i != j && dupe_names[i] == dupe_names[j])
                        {
                            BT_ERROR("i wasnt sure this would happen but it did.");
                            must_abort = true;
                        }

                // Perform the duplication!
                if (!must_abort)
                {
                    for (size_t i = 0; i < elems_to_duplicate.size(); i++)
                    {
                        // Do basic copies.
                        static auto const k_copy_elem = [](UI::UI_element const& elem_to_copy,
                                                           std::string const& new_name) {
                            UI::UI_element elem_copy = elem_to_copy;
                            elem_copy.name = new_name;

                            s_ui_editor->add_elem_to_editing_canvas(std::move(elem_copy));
                        };
                        k_copy_elem(*elems_to_duplicate[i], dupe_names[i]);
                    }

                    for (size_t i = 0; i < elems_to_duplicate.size(); i++)
                    {
                        // Set the parents correctly.
                        static auto const k_bind_parent = [](std::string const& child_name,
                                                             std::string const& parent_name) {
                            UI::UI_element* child{nullptr};
                            UI::UI_element* parent{nullptr};
                            for (auto* elem : s_ui_editor->get_editing_elems())
                            {
                                if (elem->name == child_name)
                                {
                                    child = elem;
                                }
                                if (elem->name == parent_name)
                                {
                                    parent = elem;
                                }
                            }

                            assert(child != nullptr);
                            assert(parent != nullptr || parent_name == "");
                            assert(child != parent);

                            child->parent = parent;
                        };
                        k_bind_parent(dupe_names[i], dupe_parent_names[i]);
                    }
                }

                s_ui_editor->sort_editing_canvas_elems();

                s_ui_editor->force_ui_redraw();
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete elem"))
            {
                s_ui_editor->delete_selected_editing_elem();
                s_ui_editor->sort_editing_canvas_elems();

                s_ui_editor->force_ui_redraw();
            }

            ImGui::EndDisabled();

            ImGui::Separator();

            ImGui::BeginDisabled(s_ui_editor->get_selected_editing_elem() == nullptr);

            if (ImGui::Button("move up"))
            {
                s_ui_editor->move_selected_editing_elem(-1);
                s_ui_editor->sort_editing_canvas_elems();

                s_ui_editor->force_ui_redraw();
                s_ui_editor->mark_dirty();
            }

            ImGui::SameLine();
            if (ImGui::Button("move down"))
            {
                s_ui_editor->move_selected_editing_elem(1);
                s_ui_editor->sort_editing_canvas_elems();

                s_ui_editor->force_ui_redraw();
                s_ui_editor->mark_dirty();
            }

            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::Text("%zu elements", s_ui_editor->get_editing_elems().size());

            ImGui::SeparatorText("UI element hierarchy");
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

        {
            // Edit strings.
            static std::string s_editing_str;

            using Field_check_fn_t = std::function<std::string(std::string const&)>;
            static Field_check_fn_t s_str_editing_field_check_fn;

            using Confirm_fn_t = std::function<void(std::string const&)>;
            static Confirm_fn_t s_str_editing_confirm_fn;

            ImGui::Text("name: %s", editing_elem->name.c_str());
            ImGui::SameLine();
            if (ImGui::Button("edit..##edit_name_btn"))
            {
                s_editing_str = editing_elem->name;

                s_str_editing_field_check_fn = [](std::string const& text) -> std::string {
                    for (auto* elem : s_ui_editor->get_editing_elems())
                        if (elem->name == text)
                        {
                            return "Name must be unique.";
                        }
                    return "";
                };

                s_str_editing_confirm_fn = [&editing_elem](std::string const& text) -> void {
                    editing_elem->name = text;
                };

                ImGui::OpenPopup("edit_str_field_popup");
            }

            ImGui::Text("parent: %s",
                        editing_elem->parent == nullptr ? "<NO_PARENT>"
                                                        : editing_elem->parent->name.c_str());
            ImGui::SameLine();
            if (ImGui::Button("edit..##edit_parent_btn"))
            {
                s_editing_str = (editing_elem->parent == nullptr ? "" : editing_elem->parent->name);

                s_str_editing_field_check_fn = [&editing_elem](std::string const& text) -> std::string {
                    if (text.empty())
                        return "";

                    for (auto* elem : s_ui_editor->get_editing_elems())
                        if (elem->name == text)
                        {
                            if (elem->name != editing_elem->name)
                                return "";
                            return "Cannot be parented to self.\nNOTE: empty string means no "
                                   "parent.";
                        }
                    return "This elem name does not match another elem in the hierarchy.\nNOTE: "
                           "empty string means no parent.";
                };

                s_str_editing_confirm_fn = [&elem_parent = editing_elem->parent](std::string const& text) -> void {
                    if (text.empty())
                    {
                        elem_parent = nullptr;
                        return;
                    }

                    for (auto* elem : s_ui_editor->get_editing_elems())
                        if (elem->name == text)
                        {
                            elem_parent = elem;
                            return;
                        }

                    s_ui_editor->sort_editing_canvas_elems();
                };

                ImGui::OpenPopup("edit_str_field_popup");
            }

            if (ImGui::BeginPopup("edit_str_field_popup"))
            {
                ImGui::InputText("##edit_str_text_input", &s_editing_str);

                std::string err_msg = s_str_editing_field_check_fn(s_editing_str);

                if (!err_msg.empty())
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", err_msg.c_str());
                }

                ImGui::Separator();

                ImGui::BeginDisabled(!err_msg.empty());

                if (ImGui::Button("Confirm##edit_str_confirm_btn"))
                {
                    s_str_editing_confirm_fn(s_editing_str);
                    edited |= true;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndDisabled();

                ImGui::SameLine();
                if (ImGui::Button("Cancel##edit_str_cancel_btn"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

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
        edited |= ImGui::Checkbox("is_msdf", &editing_elem->is_msdf);

        ImGui::PopID();

        if (edited)
        {
            s_ui_editor->force_ui_redraw();
            s_ui_editor->mark_dirty();
        }
    }
    ImGui::End();
}
