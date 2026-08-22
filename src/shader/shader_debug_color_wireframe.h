#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

struct Material_organizer;  // Forward decl.
struct Render_model_data_collection;  // Forward decl.
struct Render_object;  // Forward decl.
struct Render_object_model_mesh_reference;  // Forward decl.

namespace Shader
{

/// Color wireframe shader.
class Shader_debug_color_wireframe
{
public:
    static constexpr char const* k_name{ "__debug_color_wireframe" };

    Shader_debug_color_wireframe(Material_organizer& material_organizer,
                                 Render_model_data_collection& render_model_data_collection,
                                 void* graphics);
    ~Shader_debug_color_wireframe();

    void make_material(std::string const& material_name,
                       std::unordered_map<std::string, std::string> const& shader_params);
    void organize_materials();

    void allocate_per_instance_data_slots(
        std::vector<Render_object> const& render_object_list,
        std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
        size_t& in_out_cur_modmesh_ref_idx);

    void draw(std::vector<Render_object> const& render_object_list,
              std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
              void* render_view_param);

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
