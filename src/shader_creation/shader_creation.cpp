#include "shader_creation.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
using json = nlohmann::json;


namespace
{

/// Base dir for shaders.
static std::string s_shader_dir;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Reflection structs.

// Entry points.

struct Binding
{
    std::string kind;
    int32_t index;
    int32_t count{ 1 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Binding, kind, index, count);
};

struct NamedBinding
{
    std::string name;
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NamedBinding, name, binding);
};

struct Entry_point
{
    std::string name;
    std::string stage;

    std::vector<NamedBinding> bindings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Entry_point, name, stage, bindings);
};

// Parameters.

struct Param_type
{

};

struct Parameter
{
    std::string name;
    Binding binding;
    Param_type type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parameter, name, binding, type);
};






// /// Reflection struct for slang shader.
// struct Slang_shader_reflection_struct
// {
//     struct Parameter
//     {   /// @TODOL START HEREEEEEEEEJEJEJEJEJJEJEJEJEJEJEJE!!!!!!!!!!
//         struct Binding
//         {
//             std::string kind;
//             int32_t index;
//             int32_t count;
//         } binding;

//         struct Type
//         {
//             std::string kind;

//             /// Field for parameter for a shader stage.
//             struct Field
//             {
//                 std::string name;

//             };
//             std::vector<Field> fields;
//         } type;
//     };
//     std::vector<Parameter> parameters;

//     struct Entry_point
//     {
//         std::string name;
//         std::string stage;

//     };
//     std::vector<Entry_point> entryPoints;


//     // @TODO: add nlohmann json stuff here.
// };

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

    std::vector<Entry_point> const entry_points = data["entryPoints"];
    std::vector<Parameter> const parameters     = data["parameters"];

    if (is_compute_shader)
    {   // Look for compute entrypoint.
        Entry_point const* compute_ep{ nullptr };
        for (auto const& ep : entry_points)
        {
            if (ep.stage == "compute")
            {
                compute_ep = &ep;
                break;
            }
        }
        if (compute_ep == nullptr)
            throw std::runtime_error("Desired entry point not found.");

        std::cout << "Found compute entrypoint.\n";

        // Look for descriptor layouts.
        std::vector<std::pair<uint32_t, json>> descriptor_bindings;
        for (auto const& binding : compute_ep->bindings)
        {   // Look for corresponding binding param.
            Parameter const* binding_param{ nullptr };
            for (auto const& param : parameters)
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
        }

        std::cout << "Found compute entrypoint.\n";

    }

    // Extract vertex attributes.

    // Extract parameters.

    // Name.

    // Type.

    // Bind position.
}
