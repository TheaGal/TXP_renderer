#pragma once

#include "reflection_structs.h"

#include <string>


namespace TXP
{
namespace Shader_Creation
{

/// Sets the asset directory for shaders.
void set_shader_directory(std::string const& dir_path);

Reflection::Shader_reflection read_slang_reflection(std::string const& shader_name);

std::string get_shader_module_path(std::string const& shader_name);

}  // namespace Shader_Creation
}  // namespace TXP
