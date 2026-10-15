// clang-format off
#include "txp_renderer/ui/ui_state.h"
// clang-format on

#if TXP_GFX_BACKEND_VULKAN
#include "renderer/gfx_vulkan_impl.h"
#endif // TXP_GFX_BACKEND_VULKAN

#include "btdatecheck.h"
#include "btservice_finder.h"
#include "ui/ui_types.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace
{

static std::string s_ui_directory;

#if TXP_GFX_BACKEND_VULKAN
static TXP::Graphics::Impl* s_gfx;
#endif // TXP_GFX_BACKEND_VULKAN

} // namespace


void set_ui_directory(std::string const& dir_name)
{
    s_ui_directory = dir_name;
}

std::vector<std::string> fetch_all_ui_files()
{
    std::vector<std::string> fnames;

    for (auto const& p : std::filesystem::recursive_directory_iterator(s_ui_directory))
    {
        if (p.is_regular_file() && p.path().extension() == ".btui")
            fnames.emplace_back(
                std::filesystem::relative(p.path(), s_ui_directory).relative_path().string());
    }

    return fnames;
}


void set_ui_gfx_reference(void* graphics)
{
#if TXP_GFX_BACKEND_VULKAN
    s_gfx = static_cast<TXP::Graphics::Impl*>(graphics);
#else // ^^ TXP_GFX_BACKEND_VULKAN / UNKNOWN_TYPE vv
    #error Unknown GFX type.
#endif // UNKNOWN_TYPE
}


std::vector<std::pair<std::string, uint32_t>> gfx_helper::fetch_texture_name_w_idx_list()
{
    std::vector<std::pair<std::string, uint32_t>> tex_name_w_idx_list;
    tex_name_w_idx_list.reserve(s_gfx->texture_entries.size());

    for (auto const& [tex_name, entry] : s_gfx->texture_entries)
    {
        tex_name_w_idx_list.emplace_back(tex_name, entry.gpu_idx);
    }

    return tex_name_w_idx_list;
}


// class UI_element_state
void UI_element_state::add_event(UI_event event_type, std::function<void(void)>&& callback_fn)
{
    if (m_event_map.find(event_type) != m_event_map.end())
    {
        throw std::runtime_error("Shouldn't have duplicate event callbacks.");
    }

    m_event_map[event_type] = std::move(callback_fn);
}

void UI_element_state::set_position(float_t x, float_t y)
{
    if (m_loaded_elem == nullptr)
    {
        throw std::runtime_error("elem isnt loaded yet.");
    }

    m_loaded_elem->transform.x = x;
    m_loaded_elem->transform.y = y;

    m_ui_state.invalidate_draw_cache();
}

void UI_element_state::set_opacity(float_t alpha)
{
    if (m_loaded_elem == nullptr)
    {
        throw std::runtime_error("elem isnt loaded yet.");
    }

    m_loaded_elem->opacity = alpha;

    m_ui_state.invalidate_draw_cache();
}


// class UI_canvas_state
UI_element_state& UI_canvas_state::elem(std::string const& elem_name)
{
    static_assert(sizeof(UI_element_pun) == sizeof(UI::UI_element),
                  "Pun type and actual type must be the same size");
    static_assert(std::alignment_of<UI_element_pun>::value ==
                      std::alignment_of<UI::UI_element>::value,
                  "Pun type and actual type must be the same alignment");

    if (m_elem_states_map.find(elem_name) == m_elem_states_map.end())
        m_elem_states_map.emplace(elem_name, UI_element_state{ m_ui_state });
    return m_elem_states_map.at(elem_name);
}


// class UI_state
UI_state::UI_state()
{
    BT_SERVICE_FINDER_ADD_SERVICE(UI_state, this);
}

UI_canvas_state& UI_state::canvas(std::string const& canvas_name)
{
    if (m_canvas_states.find(canvas_name) == m_canvas_states.end())
        m_canvas_states.emplace(canvas_name, UI_canvas_state{ *this });
    return m_canvas_states.at(canvas_name);
}

void UI_state::load_canvas_to_front(std::string const& canvas_name)
{
    m_staged_actions.emplace_back(Stage_action::LOAD_CANVAS_TO_FRONT, canvas_name);
}

void UI_state::unload_front_canvas()
{
    m_staged_actions.emplace_back(Stage_action::UNLOAD_FRONT_CANVAS, "");
}

void UI_state::load_persistent_canvas(std::string const& canvas_name)
{
    m_staged_actions.emplace_back(Stage_action::LOAD_PERSISTENT_CANVAS, canvas_name);
}

void UI_state::unload_persistent_canvas(std::string const& canvas_name)
{
    m_staged_actions.emplace_back(Stage_action::UNLOAD_PERSISTENT_CANVAS, canvas_name);
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

UI::UI_file_data load_file_data(std::string const& canvas_name)
{
    std::ifstream f{ s_ui_directory + canvas_name };
    if (!f.is_open())
    {
        throw std::runtime_error("Opening file failed.");
    }

    return json::parse(f);
}

void save_file_data(std::string const& canvas_name, UI::UI_file_data const& file_data)
{
    std::ofstream f{ s_ui_directory + canvas_name };
    if (!f.is_open())
    {
        throw std::runtime_error("Opening file failed.");
    }

    json file_data_as_json(file_data);
    f << file_data_as_json;
}

} // namespace


void UI_state::save_persistent_canvas_to_disk(std::string const& canvas_name) const
{
    auto& canvas_state{ m_canvas_states.at(canvas_name) };

    assert(canvas_state.is_persistent() && canvas_state.is_loaded());

    auto const& my_elems{ reinterpret_cast<std::vector<UI::UI_element> const&>(
        canvas_state.m_elems) };

    // Create map to texture names.
    std::unordered_map<uint32_t, std::string> gpu_idx_to_tex_name_map;
    gpu_idx_to_tex_name_map.reserve(s_gfx->texture_entries.size());
    for (auto const& [tex_name, tex_entry] : s_gfx->texture_entries)
    {
        gpu_idx_to_tex_name_map[tex_entry.gpu_idx] = tex_name;
    }

    // Create file data.
    UI::UI_file_data file_data;

    for (auto const& elem : my_elems)
    {
        file_data.elements.emplace_back(UI::UI_file_element_data{
            .name = elem.name,
            .parent = (elem.parent == nullptr ? "" : elem.parent->name),
            .transform = elem.transform,
            .opacity = elem.opacity,
            .image = gpu_idx_to_tex_name_map.at(elem.texture_idx),
            .is_nine_slice = elem.is_nine_slice,
        });
    }

    // Write data to file.
    save_file_data(canvas_name, file_data);
}

void UI_state::tick()
{
    for (auto&& [action_type, canvas_name] : m_staged_actions)
    {
        UI_canvas_state* canvas_state_ptr{ nullptr };
        if (!canvas_name.empty())
        {
            assert_that_canvas_exists(m_canvas_states, canvas_name);
            canvas_state_ptr = &m_canvas_states.at(canvas_name);
        }

        switch (action_type)
        {
        case Stage_action::LOAD_CANVAS_TO_FRONT:
        {
            load_canvas(*canvas_state_ptr, canvas_name, false, m_loaded_canvases);
            break;
        }

        case Stage_action::UNLOAD_FRONT_CANVAS:
        {
            std::string const& frontmost_canvas_name{ m_loaded_canvases.back() };
            auto& frontmost_canvas{ m_canvas_states.at(frontmost_canvas_name) };

            if (frontmost_canvas.is_persistent())
            {
                throw std::runtime_error("Wrong unload function.");
            }

            unload_canvas(frontmost_canvas, frontmost_canvas_name, m_loaded_canvases);
            break;
        }

        case Stage_action::LOAD_PERSISTENT_CANVAS:
        {
            load_canvas(*canvas_state_ptr, canvas_name, true, m_loaded_persistent_canvases);
            break;
        }

        case Stage_action::UNLOAD_PERSISTENT_CANVAS:
        {
            if (!canvas_state_ptr->is_persistent())
            {
                throw std::runtime_error("Wrong unload function.");
            }

            unload_canvas(*canvas_state_ptr, canvas_name, m_loaded_persistent_canvases);
            break;
        }

        default: assert(false); break;
        }

        invalidate_draw_cache();
    }
    m_staged_actions.clear();
}

std::vector<UI::UI_element*> UI_state::gather_rendering_ui_elements_in_render_order() const
{
    std::vector<UI::UI_element*> render_ui_elems;

    // Add UI elements back to front.
    for (auto const& loaded_canvases_list : { m_loaded_canvases, m_loaded_persistent_canvases })
    {
        for (auto const& canvas_name : loaded_canvases_list)
            for (auto& elem : m_canvas_states.at(canvas_name).m_elems)
                render_ui_elems.emplace_back(reinterpret_cast<UI::UI_element*>(
                    const_cast<UI_canvas_state::UI_element_pun*>(&elem)));
    }

    return render_ui_elems;
}

/*static*/ void UI_state::load_canvas(UI_canvas_state& canvas_state,
                                      std::string const& canvas_name,
                                      bool const is_persistent,
                                      std::vector<std::string>& loaded_canvases_list)
{
    if (canvas_state.is_loaded())
    {
        throw std::runtime_error("Canvas is already loaded.");
    }

    // Set flags.
    canvas_state.m_is_loaded = true;
    canvas_state.m_is_persistent = is_persistent;
    canvas_state.m_load_idx = 0;

    // Load in canvas.
    UI::UI_file_data ui_file_data{ load_file_data(canvas_name) };

    std::unordered_map<std::string, uint32_t> temp_elem_name_to_idx;
    temp_elem_name_to_idx.reserve(ui_file_data.elements.size());

    auto& my_elems{ reinterpret_cast<std::vector<UI::UI_element>&>(canvas_state.m_elems) };

    my_elems.clear();
    my_elems.reserve(ui_file_data.elements.size());

    for (auto& file_elem : ui_file_data.elements)
    {
        temp_elem_name_to_idx[file_elem.name] = my_elems.size();

        my_elems.emplace_back(UI::UI_element{
            .name = file_elem.name,
            .parent = nullptr,  // Will fill in later.
            .transform = file_elem.transform,
            .opacity = file_elem.opacity,
            .texture_idx =
                static_cast<uint32_t>(s_gfx->texture_entries.at(file_elem.image).gpu_idx),
            .is_nine_slice = file_elem.is_nine_slice,
        });

        if (temp_elem_name_to_idx.size() != my_elems.size())
            throw std::runtime_error("These sizes should be the same.");
    }

    if (my_elems.size() != ui_file_data.elements.size())
        throw std::runtime_error("uh oh.");

    // Build in parenting with elements.
    for (size_t i = 0; i < my_elems.size(); i++)
    {
        auto& my_elem{ my_elems[i] };
        auto& file_elem{ ui_file_data.elements[i] };

        if (temp_elem_name_to_idx.find(file_elem.parent) != temp_elem_name_to_idx.end())
        {
            uint32_t parent_elem_idx{ temp_elem_name_to_idx.at(file_elem.parent) };
            my_elem.parent = &my_elems[parent_elem_idx];
        }
        else
        {
            my_elem.parent = nullptr;
        }
    }

    // Fill in loaded element reference.
    for (auto& my_elem : my_elems)
    {
        auto& elem_state{ canvas_state.elem(my_elem.name) };  // creates element state if didn't exist.

        if (elem_state.m_loaded_elem != nullptr)
        {
            std::runtime_error("Reference unloading didn't happen or collision?!?!");
        }

        elem_state.m_loaded_elem = &my_elem;
    }

    // Add to list.
    loaded_canvases_list.emplace_back(canvas_name);
}

/*static*/ void UI_state::unload_canvas(UI_canvas_state& canvas_state,
                                        std::string const& canvas_name,
                                        std::vector<std::string>& loaded_canvases_list)
{
    if (!canvas_state.is_loaded())
    {
        throw std::runtime_error("Trying to unload already unloaded canvas.");
    }

    // Set flags.
    canvas_state.m_is_loaded = false;
    canvas_state.m_is_persistent = false;
    canvas_state.m_load_idx = -1;

    // Remove loaded data.
    canvas_state.m_elems.clear();

    for (auto&& [_, elem_state] : canvas_state.m_elem_states_map)
    {
        if (elem_state.m_loaded_elem == nullptr)
        {
            throw std::runtime_error(
                "Trying to unload something that was never loaded or collision?!?");
        }

        elem_state.m_loaded_elem = nullptr;  // this is manipulating the ref `elem_state`.
    }

    // Remove from list.
    loaded_canvases_list.erase(
        std::remove(loaded_canvases_list.begin(), loaded_canvases_list.end(), canvas_name),
        loaded_canvases_list.end());
}

} // namespace TXP
