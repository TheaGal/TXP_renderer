#include "shader_creation.h"

#include "reflection_structs.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "nlohmann/detail/macro_scope.hpp"
#include "nlohmann/json.hpp"
using json = nlohmann::json;


namespace
{

using namespace TXP::Shader_Creation;

/// Base dir for shaders.
static std::string s_shader_dir;

/// Collection of loaded shader reflections.
static std::unordered_map<std::string, Reflection::Shader_reflection> s_shader_name_to_reflection;

}  // namespace


void TXP::Shader_Creation::set_shader_directory(std::string const& dir_path)
{
    if (dir_path.back() != '/')
        throw std::runtime_error("Must end with \"/\"");
    s_shader_dir = dir_path;
}

void TXP::Shader_Creation::clear_slang_reflection_collection()
{
    s_shader_name_to_reflection.clear();
}

void TXP::Shader_Creation::load_slang_reflection_into_collection(std::string const& shader_name)
{
    // Load the shader reflection.
    std::ifstream f{ s_shader_dir + shader_name + ".shadrefl" };
    s_shader_name_to_reflection.emplace(shader_name, json::parse(f));
}

void TXP::Shader_Creation::extract_stuff(std::string const& shader_name,
                                         Shader_pipeline_type shader_type)
{
    auto shader_refl = s_shader_name_to_reflection.at(shader_name);

    // Look for shader entrypoints depending on the type.
    std::vector<Reflection::Entry_point const*> desired_eps;
    {
        std::vector<std::string> ep_stage_names;

        switch (shader_type)
        {
        case SHAD_PIPE_TYPE_COMPUTE:
            ep_stage_names.emplace_back("compute");
            break;

        case SHAD_PIPE_TYPE_VERTEX_FRAGMENT:
            ep_stage_names.emplace_back("vertex");
            ep_stage_names.emplace_back("fragment");
            break;

        default:
            throw std::runtime_error("Invalid Shader_pipeline_type.");
        }

        for (auto const& ep_stage_name : ep_stage_names)
        {
            for (auto const& ep : shader_refl.entryPoints)
            {
                if (ep.stage == ep_stage_name)
                {
                    desired_eps.emplace_back(&ep);
                    break;
                }
            }
        }
        if (desired_eps.size() != ep_stage_names.size())
            throw std::runtime_error("Desired entry points not found.");
    }

    // Look for descriptor layouts.
    for (auto const ep : desired_eps)
    {
        std::vector<std::pair<uint32_t, json>> descriptor_bindings;
        for (auto const& binding : ep->bindings)
        {   // Look for corresponding binding param.
            Reflection::Parameter const* binding_param{ nullptr };
            for (auto const& param : shader_refl.parameters)
            {
                if (param.name == binding.name)
                {
                    binding_param = &param;
                    break;
                }
            }
            if (binding_param == nullptr)
                throw std::runtime_error("Corresponding binding param not found.");

            // asdfasdf.
            binding_param->type.kind;
        }
    }

    std::cout << "Found compute entrypoint.\n";




    // Extract vertex attributes.

    // Extract parameters.

    // Name.

    // Type.

    // Bind position.
}
