#pragma once

#include <cassert>
#include <cstdint>
#include <vector>


namespace TXP
{

/// Set of data to define a static model.
struct Static_model_data_set
{
    std::vector<Mesh> meshes;
    std::vector<Vertex> vertices;
    AA_bounding_box model_aabb;
};

/// Renderable model (to be used by render object).
struct Render_model
{
    uint16_t static_model_data_set_idx;

    uint16_t deformed_model_skin_idx{ (uint16_t)-1 };  // opt: -1 means non-deformed model.
    uint16_t deformed_vertex_buffer_idx{ (uint16_t)-1 };  // opt: -1 means non-deformed model.

    uint16_t default_material_set_idx;  // @NOTE: created from material names inside the model.

    /// Checks whether is a deformed model or not.
    bool is_deformed_model()
    {
        assert((deformed_model_skin_idx == (uint16_t)-1) ==
               (deformed_vertex_buffer_idx == (uint16_t)-1));

        return (deformed_model_skin_idx != (uint16_t)-1) &&
               (deformed_vertex_buffer_idx != (uint16_t)-1);
    }
};

}  // namespace TXP
