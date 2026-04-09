#pragma once

#if TXP_GFX_BACKEND_VULKAN
#include "renderer/gfx_vulkan/vk_buffer.h"
#endif // TXP_GFX_BACKEND_VULKAN
#include "vertex.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>


namespace TXP
{

struct Material_organizer;  // Forward decl.

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

/// Set of first indexes to start in the index buffer.
/// @NOTE: must match number of meshes in the model.
using Per_mesh_first_index_offset_set = std::vector<uint32_t>;

/// Set of data to define a static model.
struct Static_model_data_set
{
    std::vector<Mesh> meshes;
    std::vector<Vertex> vertices;
    AA_bounding_box model_aabb;

    /// First indexes to start in the index buffer.
    // @NOTE: calculated in `upload_model_entries_to_gpu()` call.
    Per_mesh_first_index_offset_set first_index_offsets;

    /// Value added to the vertex index before indexing into the vertex buffer.
    // @NOTE: calculated in `upload_model_entries_to_gpu()` call.
    int32_t vertex_index_offset;
};

struct Deformed_model_skin;  // Forward decl.

/// Deformed model based off static model.
struct Deformed_model_data_set
{
    uint16_t base_static_model_idx;
    Static_model_data_set deformed_model;
    Deformed_model_skin const& model_skin;
#if TXP_GFX_BACKEND_VULKAN
    Vk_Buffer::Allocated_buffer joint_transforms_buffer;
#endif // TXP_GFX_BACKEND_VULKAN
};

/// Renderable model (to be used by render object).
struct Render_model  // @THEA: @TODO: delete this if remains unused  -Thea 2026/04/02
{
    uint16_t static_model_data_set_idx;

    uint16_t deformed_model_skin_idx{ (uint16_t)-1 };                                // optional: -1 means non-deformed model.
    uint16_t deformed_model_anim_set_idx{ (uint16_t)-1 };                            // optional: -1 means non-deformed model.
    uint16_t deformed_vertex_buffer_idx{ (uint16_t)-1 };                             // optional: -1 means non-deformed model.
    Per_mesh_first_index_offset_set override_first_index_offsets;                    // optional: empty means non-deformed model.
    int32_t override_vertex_index_offset{ std::numeric_limits<int32_t>::lowest() };  // optional: INT_LOWEST means non-deformed model.

    uint16_t default_material_palette_idx;  // @NOTE: created from material names inside the model.

    /// Checks whether is a deformed model or not.
    bool is_deformed_model()
    {
        assert((deformed_model_skin_idx == (uint16_t)-1) ==
               (deformed_model_anim_set_idx == (uint16_t)-1) ==
               (deformed_vertex_buffer_idx == (uint16_t)-1) ==
               (override_first_index_offsets.empty()) ==
               (override_vertex_index_offset == std::numeric_limits<int32_t>::lowest()));

        return (deformed_model_skin_idx != (uint16_t)-1) &&
               (deformed_model_anim_set_idx != (uint16_t)-1) &&
               (deformed_vertex_buffer_idx != (uint16_t)-1) &&
               (!override_first_index_offsets.empty()) &&
               (override_vertex_index_offset != std::numeric_limits<int32_t>::lowest());
    }
};

// Forward declaration.
struct Render_model_data_collection;

/// Loads model.
void load_model_from_disk(Render_model_data_collection& data_collection,
                          Material_organizer& material_organizer,
                          std::string const& model_name,
                          std::string const& file_ext);

}  // namespace TXP
