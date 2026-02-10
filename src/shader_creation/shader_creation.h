#pragma once

#include <string>


namespace TXP
{
namespace Shader_Creation
{

void set_shader_directory(std::string const& dir_path);

/// Loads slang reflection data for a shader.
/// @param is_compute_shader if true, will look for a vertex stage and fragment stage.
///                          if false, will look for a compute stage.
void load_slang_reflection(std::string const& shader_name, bool is_compute_shader);

}  // namespace Shader_Creation
}  // namespace TXP
