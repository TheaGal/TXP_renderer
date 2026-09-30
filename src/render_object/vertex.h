#pragma once

#include "btglm.h"

#include <cstdint>
#include <type_traits>
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
/// @NOTE: must be 16-byte aligned to best match std430.
/// @NOTE: all scalars so that no vec3 are used (which get turned into vec4).
struct alignas(16) Vertex
{
    float_t position_x;
    float_t position_y;
    float_t position_z;
    float_t normal_x;
    float_t normal_y;
    float_t normal_z;
    float_t uv_x;
    float_t uv_y;
    float_t tangent_x;
    float_t tangent_y;
    float_t tangent_z;
    float_t tangent_w;

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

    /// Handle of vec4 for tangent attribute.
    float_t* tangent_vec4()
    {
        return &tangent_x;
    }
};
static_assert(sizeof(Vertex) % 16 == 0, "Size must be multiple of 16 bytes");
static_assert(std::alignment_of<Vertex>::value == 16, "Must be 16 byte aligned");

/// Vertex-level deformation data to point to joint indices.
struct Vertex_skin_data
{
    uint32_t joint_mat_idxs[4];
    vec4     weights;
};

}  // namespace TXP
