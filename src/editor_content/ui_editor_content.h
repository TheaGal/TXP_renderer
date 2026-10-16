#pragma once

#include <string>
#include <vector>


namespace TXP
{

class UI_state;  // forward decl.

namespace UI
{
struct UI_element;  // forward decl.
} // namespace UI

namespace editor_content
{
namespace ui_editor
{

/// .
class UI_editor
{
public:
    UI_editor(UI_state& ui_ref);
    ~UI_editor();

    void force_ui_redraw();

    void load_canvas_to_view(std::string const& canvas_name);
    void unload_viewing_canvas(std::string const& canvas_name);

    int32_t get_editing_canvas_idx() const;
    void build_editing_elems_for_canvas_idx(int32_t const canvas_idx, bool const force_reload);

    void add_elem_to_editing_canvas(UI::UI_element&& elem);
    void save_editing_canvas() const;

    std::vector<UI::UI_element*>& get_editing_elems();

    void set_selected_editing_elem(UI::UI_element* elem);
    UI::UI_element* get_selected_editing_elem();
    void delete_selected_editing_elem();

    void mark_dirty();
    bool is_dirty() const;
    void make_not_dirty_also_known_as_clean();

private:
    UI_state& m_ui_ref;

    struct Pre_hijack_ui_state
    {
        std::vector<std::string> loaded_canvases;
        std::vector<std::string> loaded_persistent_canvases;
    } m_pre_hijack_state;

    int32_t m_editing_canvas_idx{ -1 };
    UI::UI_element* m_editing_canvas_element_ptr{ nullptr };
    std::vector<UI::UI_element*> m_editing_elems;

    bool m_is_data_dirty{ false };
};

void open_editor();

void close_editor();

void ui_editor_content();

} // namespace ui_editor
} // namespace editor_content
} // namespace TXP
