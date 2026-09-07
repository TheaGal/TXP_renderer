// clang-format off
#include "txp_renderer/ui/ui_state.h"
// clang-format on

#include "ui/ui_types.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>


namespace TXP
{

// class UI_element_state
void UI_element_state::add_event(UI_event event_type, std::function<void(void)>&& callback_fn)
{
    if (m_event_map.find(event_type) != m_event_map.end())
    {
        throw std::runtime_error("Shouldn't have duplicate event callbacks.");
    }

    m_event_map[event_type] = std::move(callback_fn);
}


// class UI_canvas_state
UI_element_state& UI_canvas_state::elem(std::string const& elem_name)
{
    return m_elem_states_map[elem_name];
}


// class UI_state
UI_canvas_state& UI_state::canvas(std::string const& canvas_name)
{
    return m_canvas_states[canvas_name];
}

namespace
{

void assert_that_canvas_exists(
    std::unordered_map<std::string, UI_canvas_state> const& canvas_states,
    std::string const& canvas_name)
{
    if (canvas_states.find(canvas_name) == canvas_states.end())
    {
        throw std::runtime_error("Canvas state does not exist.");
    }
}

UI::UI_file_data load_file_data(std::string const& file_name)
{
    std::ifstream f{ file_name };
    if (!f.is_open())
    {
        throw std::runtime_error("Opening file failed.");
    }

    return json::parse(f);
}

} // namespace

void UI_state::load_canvas_to_front(std::string const& canvas_name)
{
    assert_that_canvas_exists(m_canvas_states, canvas_name);
}

void UI_state::unload_front_canvas()
{
    assert(false);
}

void UI_state::load_persistent_canvas(std::string const& canvas_name)
{
    assert_that_canvas_exists(m_canvas_states, canvas_name);

    auto& my_canvas{ canvas(canvas_name) };

    if (my_canvas.is_loaded())
    {
        throw std::runtime_error("Canvas is already loaded.");
    }

    // Set flags.
    my_canvas.m_is_loaded = true;
    my_canvas.m_is_persistent = true;
    my_canvas.m_load_idx = 0;

    // Load in canvas.
    UI::UI_file_data ui_file_data{ load_file_data(canvas_name) };

    std::unordered_map<std::string, uint32_t> temp_elem_name_to_idx;
    temp_elem_name_to_idx.reserve(ui_file_data.elements.size());

    my_canvas.m_elems.clear();
    my_canvas.m_elems.reserve(ui_file_data.elements.size());

    for (auto& file_elem : ui_file_data.elements)
    {
        temp_elem_name_to_idx[file_elem.name] = my_canvas.m_elems.size();

        my_canvas.m_elems.emplace_back(UI::UI_element{
            .name = file_elem.name,
            .parent = nullptr,  // Will fill in later.
            .transform = file_elem.transform,
            .opacity = file_elem.opacity,
            .texture_idx = m_pimpl->g.texture_entries.at(file_elem.image).gpu_idx,
        });

        if (temp_elem_name_to_idx.size() != my_canvas.m_elems.size())
            throw std::runtime_error("These sizes should be the same.");
    }

    if (my_canvas.m_elems.size() != ui_file_data.elements.size())
        throw std::runtime_error("uh oh.");

    // Build in parenting with elements.
    for (size_t i = 0; i < my_canvas.m_elems.size(); i++)
    {
        auto& my_elem{ my_canvas.m_elems[i] };
        auto& file_elem{ ui_file_data.elements[i] };

        uint32_t parent_elem_idx{ temp_elem_name_to_idx.at(file_elem.parent) };
        
        my_elem.parent = &my_canvas.m_elems[parent_elem_idx];
    }

    // Fill in loaded element reference.
    for (auto& my_elem : my_canvas.m_elems)
    {
        if (my_canvas.m_elem_states_map.find(my_elem.name) != my_canvas.m_elem_states_map.end())
        {
            auto& elem_state{ my_canvas.m_elem_states_map.at(my_elem.name) };

            if (elem_state.m_loaded_elem != nullptr)
            {
                std::runtime_error("Reference unloading didn't happen or collision?!?!");
            }

            elem_state.m_loaded_elem = &my_elem;
        }
    }
}

void UI_state::unload_persistent_canvas(std::string const& canvas_name)
{
    assert_that_canvas_exists(m_canvas_states, canvas_name);

    auto& my_canvas{ canvas(canvas_name) };

    if (!my_canvas.is_loaded() || !my_canvas.is_persistent())
    {
        throw std::runtime_error("Wrong unload function.");
    }

    // Set flags.
    my_canvas.m_is_loaded = false;
    my_canvas.m_is_persistent = false;
    my_canvas.m_load_idx = -1;

    // Remove loaded data.
    my_canvas.m_elems.clear();

    for (auto&& [_, elem_state] : my_canvas.m_elem_states_map)
    {
        if (elem_state.m_loaded_elem == nullptr)
        {
            throw std::runtime_error(
                "Trying to unload something that was never loaded or collision?!?");
        }

        elem_state.m_loaded_elem = nullptr;  // @CHECK: that this is manipulating the reference and not a copy.
    }
}

void UI_state::tick()
{
    assert(false);
}

} // namespace TXP
