#pragma once

#include "btglm.h"

#include <string>


namespace TXP
{
namespace debug
{

using debug_model_id_t = uint64_t;

enum Material_type
{
    PHYSICS_WIREFRAME,
    SELECTED_WIREFRAME,
    NUM_MATERIAL_TYPES
};

/// Adds render job for a model.
debug_model_id_t emplace_debug_model(std::string const& model_name, Material_type material);

/// Removes render job for a model.
void remove_debug_model(debug_model_id_t model_id);

/// Updates transform of model.
void update_debug_model_transform(debug_model_id_t model_id, mat4 transform);

/// Line struct.
struct Debug_line
{
    vec3 pos1;
    vec3 pos2;
    vec4 color1;
    vec4 color2;
};

/// Submits one debug line for a certain amount of time.
void emplace_debug_line(Debug_line&& line, float_t duration);

/// Emplaces a capsule made up of a bunch of debug lines in a batch into the pool. Only one color
/// param is provided for consistency.
///
/// @NOTE: `origin_a` and `origin_b` are the same operation of `BT::Hitcapsule`, in that
/// imagining the capsule as a sphere with `radius` that glides along a line segment with start
/// and end points `origin_a` and `origin_b` is the best way to imagine how the params work.
void emplace_debug_line_based_capsule(vec3 origin_a,
                                      vec3 origin_b,
                                      float_t radius,
                                      vec4 color,
                                      float_t timeout);

} // namespace debug
} // namespace TXP
