#pragma once

#include "btglm.h"
#if TXP_GFX_BACKEND_VULKAN
#include "renderer/gfx_vulkan/vk_buffer.h"
#endif // TXP_GFX_BACKEND_VULKAN
#include "vertex.h"

#include <unordered_map>
#include <vector>


namespace TXP
{

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

#if TXP_GFX_BACKEND_VULKAN
    Vk_Buffer::Allocated_buffer vert_skin_data_buffer;
#endif // TXP_GFX_BACKEND_VULKAN

    mat4 baseline_transform = GLM_MAT4_IDENTITY_INIT;
    mat4 inverse_global_transform = GLM_MAT4_IDENTITY_INIT;
    std::unordered_map<std::string, uint32_t> joint_name_to_idx;
    std::vector<Model_joint> joints_sorted_breadth_first;
};

}  // namespace TXP
