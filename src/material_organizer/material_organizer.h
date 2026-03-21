#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>


namespace TXP
{

struct Material_organizer;  // Forward decl.

/// Individual entry of material index data.
struct Material_index_entry
{
    uint16_t shader_id;
    uint16_t material_param_set_idx;
};

/// Palette of materials to be used on a render object in order.
/// @note If sizes are mismatched, loop.
class Material_palette
{
public:
    void emplace_materials(Material_organizer const& material_organizer,
                           std::vector<std::string> const& material_names_in_order);

    Material_index_entry const& at(size_t idx) const;

private:
    std::vector<Material_index_entry> m_materials;
};

/// Collection that holds material information for render objects.
struct Material_organizer
{
    Material_organizer();
    ~Material_organizer();

    void emplace_shader(std::string const& shader_name);
    uint16_t get_shader_id(std::string const& shader_name) const;

    /// Keeps track of material param idx by auto incrementing inside shader registration.
    /// @USAGE: as materials are created, execute this function.
    void emplace_material(std::string const& material_name, std::string const& shader_name);

    /// Obtains the (shader-local) material param idx for render objects.
    uint16_t get_material_param_set_idx(std::string const& material_name) const;

    /// Obtains the shader ID of the shader that the material belongs to.
    uint16_t get_shader_id_from_material_name(std::string const& material_name) const;

    /// .
    void emplace_material_palette(std::string const& material_palette_name,
                                  Material_palette&& mat_pal);

    /// Links and adds another name to access the same material palette as the original name.
    void emplace_material_palette_alias(std::string const& material_palette_alias_name,
                                        std::string const& material_palette_original_name);

    /// .
    uint16_t get_material_palette_idx(std::string const& material_palette_name) const;

    /// .
    Material_palette const& get_material_palette(uint16_t material_palette_idx) const;

    // Pimpl.
    struct Data;
    std::unique_ptr<Data> inner_data;
};

}  // namespace TXP
