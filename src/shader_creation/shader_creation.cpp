#include "shader_creation.h"

#include "reflection_structs.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/detail/macro_scope.hpp"
#include "nlohmann/json.hpp"
using json = nlohmann::json;


namespace
{

/// Base dir for shaders.
static std::string s_shader_dir;

}  // namespace


void TXP::Shader_Creation::set_shader_directory(std::string const& dir_path)
{
    if (dir_path.back() != '/')
        throw std::runtime_error("Must end with \"/\"");
    s_shader_dir = dir_path;
}

void TXP::Shader_Creation::load_slang_reflection(std::string const& shader_name,
                                                 bool is_compute_shader)
{
    // Load the shader reflection.
    std::ifstream f{ s_shader_dir + shader_name + ".shadrefl" };
    auto data = json::parse(f);

    std::vector<Reflection::Entry_point> const all_entry_points = data["entryPoints"];
    std::vector<Reflection::Parameter> const all_parameters     = data["parameters"];

    // Look for compute entrypoints.
    std::vector<Reflection::Entry_point const*> desired_eps;
    {
        std::vector<std::string> ep_stage_names;
        if (is_compute_shader)
        {
            ep_stage_names.emplace_back("compute");
        }
        else
        {
            ep_stage_names.emplace_back("vertex");
            ep_stage_names.emplace_back("fragment");
        }

        for (auto const& ep_stage_name : ep_stage_names)
        {
            for (auto const& ep : all_entry_points)
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

    std::cout << "Found entrypoint(s).\n";

    // Look for descriptor layouts.
    for (auto const ep : desired_eps)
    {
        std::vector<std::pair<uint32_t, json>> descriptor_bindings;
        for (auto const& binding : ep->bindings)
        {   // Look for corresponding binding param.
            Reflection::Parameter const* binding_param{ nullptr };
            for (auto const& param : all_parameters)
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
