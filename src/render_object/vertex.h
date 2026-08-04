#pragma once

#include "btglm.h"

#include <cstdint>
#include <vector>


namespace TXP
{

struct Mesh
{
    std::string mesh_name;
    vec3s origin_pos;  // @TODO: unused at this point???
    std::vector<uint32_t> indices;
};

/// std430 aligned vertex data.
/// @NOTE: must be 16-byte aligned.
/// @NOTE: all scalars so that no vec3 are used (which get turned into vec4).
struct Vertex
{
    float_t position_x;
    float_t position_y;
    float_t position_z;
    float_t normal_x;
    float_t normal_y;
    float_t normal_z;
    float_t uv_x;
    float_t uv_y;

    /// Handle of vec3 for position attribute.
    float_t* position_vec3()
    {
        return &position_x;
    }

    /// Handle of vec3 for normal attribute.
    float_t* normal_vec3()
    {
        return &normal_x;
    }

    /// Handle of vec2 for UV attribute.
    float_t* uv_vec2()
    {
        return &uv_x;
    }
};

/// Vertex-level deformation data to point to joint indices.
struct Vertex_skin_data
{
    uint32_t joint_mat_idxs[4];
    vec4     weights;
};

}  // namespace TXP
