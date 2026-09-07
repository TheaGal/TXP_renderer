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
    UI_canvas_state() = default;

    std::string m_name;  // needed?

    bool m_is_loaded{ false };
    bool m_is_persistent{ false };
    int32_t m_load_idx{ -1 };

    std::unordered_map<std::string, UI_element_state> m_elem_states_map;
    std::vector<UI::UI_element> m_elems;

    friend class UI_state;
};

/// .
class UI_state
{
public:
    UI_canvas_state& canvas(std::string const& canvas_name);

    void load_canvas_to_front(std::string const& canvas_name);
    void unload_front_canvas();

    void load_persistent_canvas(std::string const& canvas_name);
    void unload_persistent_canvas(std::string const& canvas_name);

    void tick();

private:
    std::unordered_map<std::string, UI_canvas_state> m_canvas_states;
};

} // namespace TXP
