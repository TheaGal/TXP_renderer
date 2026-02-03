#pragma once

#include "cglm/mat4.h"
#include "cglm/vec4.h"

#include <unordered_map>
#include <vector>


namespace TXP
{

/// Vertex-level deformation data to point to joint indices.
struct Vertex_skin_data
{
    uint32_t joint_mat_idxs[4];
    vec4     weights;
};

/// Joint within a hierarchy.
struct Model_joint
{
    std::string name;
    mat4 inverse_bind_matrix;
    uint32_t parent_idx{ (uint32_t)-1 };  // @NOTE: Idx instead of pointer for cache lookup.  -Thea 2025/07/10
    std::vector<Model_joint*> children;
};

/// Data to deform a vertex buffer of a model.
struct Deformed_model_skin
{
    std::vector<Vertex_skin_data> vert_skin_datas;  // @TODO: See if this needs to be stored CPU-side.

    mat4 baseline_transform = GLM_MAT4_IDENTITY_INIT;
    mat4 inverse_global_transform = GLM_MAT4_IDENTITY_INIT;
    std::unordered_map<std::string, uint32_t> joint_name_to_idx;
    std::vector<Model_joint> joints_sorted_breadth_first;
};

}  // namespace TXP
