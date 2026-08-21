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

} // namespace debug
} // namespace TXP
