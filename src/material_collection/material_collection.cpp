#include "material_collection.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

// struct Data
struct Material_collection::Data
{
    std::unordered_map<std::string, size_t> shader_name_to_id_map;

    std::vector<size_t> material_param_set_idx_counters;  // Corresponds to shader id.
    std::unordered_map<std::string, size_t> material_name_to_param_set_idx_map;
};

// struct Material_collection
Material_collection::Material_collection()
    : inner_data(std::make_unique<Data>())
{
}

Material_collection::~Material_collection() = default;  // For pimpl

void Material_collection::emplace_shader(std::string const& shader_name)
{
    if (inner_data->shader_name_to_id_map.find(shader_name) !=
        inner_data->shader_name_to_id_map.end())
    {
        throw std::runtime_error("Already contains this shader name in collection.");
    }

    inner_data->shader_name_to_id_map.emplace(shader_name,
                                              inner_data->shader_name_to_id_map.size());
    inner_data->material_param_set_idx_counters.emplace_back(0);
}

uint16_t Material_collection::get_shader_id(std::string const& shader_name)
{
    return static_cast<uint16_t>(inner_data->shader_name_to_id_map.at(shader_name));
}

void Material_collection::emplace_material(std::string const& material_name,
                                           std::string const& shader_name)
{
    if (inner_data->material_name_to_param_set_idx_map.find(material_name) !=
        inner_data->material_name_to_param_set_idx_map.end())
    {
        throw std::runtime_error("Material name is already in collection.");
    }

    auto shader_id = size_t{ inner_data->shader_name_to_id_map.at(shader_name) };

    auto new_param_set_idx = size_t{ inner_data->material_param_set_idx_counters[shader_id]++ };

    inner_data->material_name_to_param_set_idx_map.emplace(material_name, new_param_set_idx);
}

uint16_t Material_collection::get_material_param_set_idx(std::string const& material_name)
{
    return static_cast<uint16_t>(inner_data->material_name_to_param_set_idx_map.at(material_name));
}

void Material_collection::emplace_material_palette(std::string const& material_palette_name,
                                                   Material_palette&& mat_pal)
{
    assert(false);
}

void Material_collection::emplace_material_palette_alias(
    std::string const& material_palette_alias_name,
    std::string const& material_palette_original_name)
{
    assert(false);
}

uint16_t Material_collection::get_material_palette_idx(std::string const& material_palette_name)
{
    assert(false);
}

Material_palette const& Material_collection::get_material_palette(uint16_t material_palette_idx)
{
    assert(false);
}

}  // namespace TXP
