#include "shader_creation.h"

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


////////////////////////////////////////////////////////////////////////////////////////////////////
// Reflection structs.
// @NOTE: naming convention of vars will not match rest of project.

// Entry points.

struct Binding
{
    std::string kind;
    int32_t index;
    int32_t count{ 1 };
    int32_t offset{ 0 };
    int32_t size{ -1 };
    int32_t elementStride{ -1 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Binding,
                                                kind,
                                                index,
                                                count,
                                                offset,
                                                size,
                                                elementStride);
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

    std::vector<int32_t> threadGroupSize;  // Only in compute shader.
    std::vector<NamedBinding> bindings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Entry_point,
                                                name,
                                                stage,
                                                threadGroupSize,
                                                bindings);
};

// Parameters.

struct Vector_element_type
{
    std::string kind;
    std::string scalarType;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vector_element_type, kind, scalarType);
};

struct Field_type
{
    std::string kind;

    // matrix
    int32_t rowCount{ -1 };
    int32_t columnCount{ -1 };

    // resource
    std::string baseShape;
    std::string access;
    json resultType;  // Can convert to `Field_type` manually.

    // vector
    int32_t elementCount{ -1 };

    // matrix, vector
    Vector_element_type elementType;


    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Field_type,
                                                kind,
                                                rowCount,
                                                columnCount,
                                                baseShape,
                                                access,
                                                resultType,
                                                elementCount,
                                                elementType);
};

struct Element_type_field  // Data appear to be different depending on `elementType` or `elementVarLayout`.
{
    std::string name;
    Field_type type;
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_type_field, name, type, binding);
};

struct Element_type
{
    std::string kind;
    std::vector<Element_type_field> fields;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_type, kind, fields);
};

struct Container_var_layout
{
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Container_var_layout, binding);
};

struct Element_var_layout
{
    Element_type type;
    std::vector<Binding> bindings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_var_layout, type, bindings);
};

struct Param_type
{
    std::string kind;
    Element_type elementType;
    Container_var_layout containerVarLayout;
    Element_var_layout elementVarLayout;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Param_type,
                                   kind,
                                   elementType,
                                   containerVarLayout,
                                   elementVarLayout);
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

    std::vector<Entry_point> const all_entry_points = data["entryPoints"];
    std::vector<Parameter> const all_parameters     = data["parameters"];

    // Look for compute entrypoints.
    std::vector<Entry_point const*> desired_eps;
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
            Parameter const* binding_param{ nullptr };
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
