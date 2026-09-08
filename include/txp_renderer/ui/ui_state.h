#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace UI
{

struct UI_element;  // Forward decl.

} // namespace UI


/// Sets the directory that UI assets are stored.
/// @NOTE: should be internal
void set_ui_directory(std::string const& dir_name);

/// Sets a reference to the implementation of the graphics.
/// @NOTE: should be internal
void set_ui_gfx_reference(void* graphics);

/// .
enum UI_event : uint32_t
{
    UI_EV_ON_CONFIRM_PRESS,
    UI_EV_ON_CONFIRM_RELEASE,
    UI_EV_ON_HOVER_ENTER,

    NUM_UI_EV
};

/// .
class UI_element_state
{
public:
    void add_event(UI_event event_type, std::function<void(void)>&& callback_fn);

private:
    UI_element_state() = default;

    UI::UI_element* m_loaded_elem{ nullptr };  // Use nullptr to check if elem is loaded.
    std::unordered_map<UI_event, std::function<void(void)>> m_event_map;

    friend class UI_canvas_state;
    friend class UI_state;
};

/// .
class UI_canvas_state
{
public:
    ~UI_canvas_state();

    /// Gets or emplaces an element state.
    UI_element_state& elem(std::string const& elem_name);

    inline bool is_loaded() const
    {
        return m_is_loaded;
    }
    inline bool is_persistent() const
    {
        return m_is_persistent;
    }
    inline int32_t load_idx() const
    {
        return m_load_idx;
    }

private:
    UI_canvas_state();

    std::string m_name;  // needed?

    bool m_is_loaded{ false };
    bool m_is_persistent{ false };
    int32_t m_load_idx{ -1 };

    std::unordered_map<std::string, UI_element_state> m_elem_states_map;
    std::vector<UI::UI_element>* m_elems;  // manual ctor/dtor action.

    friend class UI_state;
};

/// .
class UI_state
{
public:
    /// Gets or emplaces a canvas state.
    UI_canvas_state& canvas(std::string const& canvas_name);

    /// Loads a canvas onto the canvas stack (0 is the top-most/front-most).
    /// @NOTE: this errors if already loaded.
    /// @NOTE: this call stages, where `.tick()` commits the staged changes.
    void load_canvas_to_front(std::string const& canvas_name);

    /// Unloads the most recently loaded canvas.
    /// @NOTE: this call stages, where `.tick()` commits the staged changes.
    void unload_front_canvas();

    /// Loads a canvas outside the canvas stack (is_loaded == true and load_idx == 0
    /// and is_persistent == true).
    /// @NOTE: this call stages, where `.tick()` commits the staged changes.
    void load_persistent_canvas(std::string const& canvas_name);

    /// Unloads a persistenly loaded canvas.
    /// @NOTE: this call stages, where `.tick()` commits the staged changes.
    void unload_persistent_canvas(std::string const& canvas_name);

    /// This must be called once per frame (ideally after all the ui_state changes, and
    /// right before `render_one_frame()`).
    void tick();

    /// For rendering. Gathers all visible and drawing UI elements.
    std::vector<UI::UI_element*> gather_rendering_ui_elements_in_render_order() const;

private:
    std::unordered_map<std::string, UI_canvas_state> m_canvas_states;

    struct Stage_action
    {
        enum Stage_action_type
        {
            LOAD_CANVAS_TO_FRONT,
            UNLOAD_FRONT_CANVAS,
            LOAD_PERSISTENT_CANVAS,
            UNLOAD_PERSISTENT_CANVAS,
        } type;

        std::string canvas_name;
    };
    std::vector<Stage_action> m_staged_actions;

    std::vector<std::string> m_loaded_canvases;
    std::vector<std::string> m_loaded_persistent_canvases;
};

} // namespace TXP
