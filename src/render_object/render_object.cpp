#include "render_object.h"

#include "btglm.h"
#include "render_object/render_model.h"
#include "txp_renderer/types.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>


namespace TXP
{

struct Render_model_data_collection::Data
{
    static constexpr size_t k_array_pool_size{ 0xFFFF };  // 2^16 - 1 (so -1 is reserved).

    std::array<Static_model_data_set, k_array_pool_size> static_model_data_set_pool;
    std::map<std::string, uint16_t> name_to_static_model_data_set_idx_map;
};

Render_model_data_collection::Render_model_data_collection()
    : inner_data(std::make_unique<Data>())
{
}

Render_model_data_collection::~Render_model_data_collection() = default;


void Render_model_data_collection::emplace_static_model_data_set(std::string const& name,
                                                                 Static_model_data_set&& data)
{
    auto& name_to_idx_map{ inner_data->name_to_static_model_data_set_idx_map };
    if (name_to_idx_map.find(name) != name_to_idx_map.end())
        throw std::runtime_error("Found duplicate key in data collection already.  name=" + name);
    
    // Check size isn't overflown.
    size_t next_ins_pos{ name_to_idx_map.size() };
    if (next_ins_pos >= Data::k_array_pool_size)
        throw std::runtime_error("Attempting to emplace into a full data collection.");

    // Perform emplace.
    inner_data->static_model_data_set_pool[next_ins_pos] = std::move(data);
    name_to_idx_map.emplace(name, static_cast<uint16_t>(next_ins_pos));
}

uint16_t Render_model_data_collection::get_static_model_data_set_idx(std::string const& name) const
{
    return inner_data->name_to_static_model_data_set_idx_map.at(name);
}

Static_model_data_set const& Render_model_data_collection::get_static_model_data_set(
    uint16_t idx) const
{
    if (idx >= inner_data->name_to_static_model_data_set_idx_map.size())
        throw std::runtime_error("Out of bounds index: " + std::to_string(idx));
    
    return inner_data->static_model_data_set_pool[idx];
}


uint16_t Render_model_data_collection::emplace_deformed_model_skin(std::string const& name, Deformed_model_skin&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_model_skin(std::string const& name)
{
    assert(false);
}

uint16_t Render_model_data_collection::emplace_deformed_model_anim_set(
    std::string const& name,
    Deformed_model_animation_set&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_model_anim_set(std::string const& name)
{
    assert(false);
}

uint16_t Render_model_data_collection::emplace_deformed_vertex_buffer(std::string const& name, void* data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_vertex_buffer(std::string const& name)
{
    assert(false);
}


uint16_t Render_model_data_collection::emplace_material_set(std::string const& name, Material_set&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_material_set(std::string const& name)
{
    assert(false);
}

}  // namespace TXP
