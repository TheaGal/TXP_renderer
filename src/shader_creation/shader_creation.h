#pragma once

#include <string>


namespace TXP
{
namespace Shader_Creation
{

void set_shader_directory(std::string const& dir_path);

void clear_slang_reflection_collection();

/// Loads slang reflection data for a shader.
/// @param is_compute_shader if true, will look for a vertex stage and fragment stage.
///                          if false, will look for a compute stage.
void load_slang_reflection_into_collection(std::string const& shader_name);

enum Shader_pipeline_type
{
    SHAD_PIPE_TYPE_COMPUTE = 0,
    SHAD_PIPE_TYPE_VERTEX_FRAGMENT,
};

void extract_stuff(std::string const& shader_name, Shader_pipeline_type shader_type);

}  // namespace Shader_Creation
}  // namespace TXP
