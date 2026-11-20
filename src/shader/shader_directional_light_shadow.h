#pragma once

#include "btglm.h"
#include "txp_renderer/types.h"

#include <cstddef>
#include <memory>
#include <vector>


namespace TXP
{

struct Material_organizer;  // Forward decl.
struct Render_model_data_collection;  // Forward decl.
struct Render_object;  // Forward decl.
struct Render_object_model_mesh_reference;  // Forward decl.

namespace Shader
{

/// Shadow shader.
class Shader_directional_light_shadow
{
public:
    static constexpr char const* k_name{ "directional_light_shadow" };

    Shader_directional_light_shadow(Material_organizer& material_organizer,
                                    Render_model_data_collection& render_model_data_collection,
                                    void* graphics);
    ~Shader_directional_light_shadow();

    void allocate_per_instance_data_slots(
        std::vector<Render_object> const& render_object_list,
        std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
        size_t& in_out_cur_modmesh_ref_idx);

    size_t get_num_shadow_atlas_entries_to_draw() const;

    void draw(size_t shadow_atlas_entry_idx,
              std::vector<Render_object> const& render_object_list,
              std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
              Render_layer const render_layer_mask);

    void wait_until_completion();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace Shader
}  // namespace TXP
