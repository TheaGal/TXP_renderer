#include "shader_creation.h"

#include "btjson.h"
#include "reflection_structs.h"

#include <fstream>
#include <stdexcept>
#include <string>


namespace
{
using namespace TXP::Shader_Creation;

/// Base dir for shaders.
static std::string s_shader_dir;

}  // namespace


void TXP::Shader_Creation::set_shader_directory(std::string const& dir_path)
{
    if (dir_path.back() != '/')
        throw std::runtime_error("Must end with \"/\"");
    s_shader_dir = dir_path;
}

Reflection::Shader_reflection TXP::Shader_Creation::read_slang_reflection(
    std::string const& shader_name)
{
    // Load the shader reflection.
    std::ifstream f{ s_shader_dir + shader_name + ".shadrefl" };
    return json::parse(f);
}

std::string TXP::Shader_Creation::get_shader_module_path(std::string const& shader_name)
{
    return (s_shader_dir + shader_name + ".shader");
}
