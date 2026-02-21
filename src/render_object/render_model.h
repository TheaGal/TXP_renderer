#pragma once

#include "vertex.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>


namespace TXP
{

/// Sets the asset directory for models.
void set_model_directory(std::string const& dir_path);

/// Bounding box.
struct AA_bounding_box
{
    vec3 min;
    vec3 max;

    void reset();
    void feed_position(vec3 position);
};

/// Set of vertex offsets when model is inside a combined index buffer.
/// "Vertex offset" is the value added to the vertex index before indexing into the vertex buffer.
using Per_mesh_vertex_index_offset_set = std::vector<int32_t>;

/// Set of data to define a static model.
struct Static_model_data_set
{
    std::vector<Mesh> meshes;
    std::vector<Vertex> vertices;
    AA_bounding_box model_aabb;

    // @NOTE: calculated in `upload_model_entries_to_gpu()` call.
    Per_mesh_vertex_index_offset_set default_per_mesh_vertex_index_offset_set;
};

/// Renderable model (to be used by render object).
struct Render_model
{
    uint16_t static_model_data_set_idx;

    uint16_t deformed_model_skin_idx{ (uint16_t)-1 };  // optional: -1 means non-deformed model.
    uint16_t deformed_model_anim_set_idx{ (uint16_t)-1 };  // optional: -1 means non-deformed model.
    uint16_t deformed_vertex_buffer_idx{ (uint16_t)-1 };  // optional: -1 means non-deformed model.
    Per_mesh_vertex_index_offset_set override_per_mesh_vertex_index_offset_set;  // optional: empty means non-deformed model.

    uint16_t default_material_set_idx;  // @NOTE: created from material names inside the model.

    /// Checks whether is a deformed model or not.
    bool is_deformed_model()
    {
        assert((deformed_model_skin_idx == (uint16_t)-1) ==
               (deformed_model_anim_set_idx == (uint16_t)-1) ==
               (deformed_vertex_buffer_idx == (uint16_t)-1) ==
               (override_per_mesh_vertex_index_offset_set.empty()));

        return (deformed_model_skin_idx != (uint16_t)-1) &&
               (deformed_model_anim_set_idx != (uint16_t)-1) &&
               (deformed_vertex_buffer_idx != (uint16_t)-1) &&
               (!override_per_mesh_vertex_index_offset_set.empty());
    }
};

// Forward declaration.
struct Render_model_data_collection;

/// Loads model.
void load_model_from_disk(Render_model_data_collection& data_collection,
                          std::string const& model_name,
                          std::string const& file_ext);

}  // namespace TXP
