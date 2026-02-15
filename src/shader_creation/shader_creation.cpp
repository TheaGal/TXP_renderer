#include "shader_creation.h"

#include "reflection_structs.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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

TXP::Shader_Creation::Extracted_info TXP::Shader_Creation::extract_stuff(
    std::string const& shader_name,
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

    // Extracting data.
    Extracted_info extract_info;

    // Look for descriptor layouts.
    extract_info.entry_points.reserve(desired_eps.size());

    for (auto const ep : desired_eps)
    {
        extract_info.entry_points.emplace_back();

        // Entry point name.
        extract_info.entry_points.back().entry_point_name = ep->name;

        if (shader_type == SHAD_PIPE_TYPE_COMPUTE)
        {   // Get the thread group size for compute shaders.
            if (ep->threadGroupSize.size() == 3)
                extract_info.entry_points.back().compute_thread_group_size = {
                    static_cast<uint32_t>(ep->threadGroupSize[0]),
                    static_cast<uint32_t>(ep->threadGroupSize[1]),
                    static_cast<uint32_t>(ep->threadGroupSize[2]),
                };
            else
                throw std::runtime_error(
                    "Inappropriate size for compute shader thread group size config.");
        }

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

            // Create descriptor layout.
            if (binding_param->type.elementType.kind != "struct")
                throw std::runtime_error("Binding param's element type kind is not \"struct\".");

            for (auto const& elem_type_field : binding_param->type.elementType.fields)
            {
                if (elem_type_field.binding.kind != "descriptorTableSlot")  // @THEA
                    throw std::runtime_error("TODO: implement other stuff other than desc table slot!!!");

                // Get descriptor binding index.
                auto binding_idx{ static_cast<uint32_t>(elem_type_field.binding.index) };

                // Get descriptor type.
                Extracted_info::Descriptor_type descriptor_type;
                if (elem_type_field.type.baseShape == "texture2D")
                {
                    if (elem_type_field.type.access == "write")
                        descriptor_type = Extracted_info::TXP_SC_DESC_TYPE_STORAGE_IMAGE;
                    else
                        std::runtime_error("TODO: unimplemented.");
                }
                else
                    std::runtime_error("TODO: unimplemented.");

                // Add to layout list.
                extract_info.entry_points.back().desc_layout_info.layout_bindings.emplace_back(
                    binding_idx,
                    descriptor_type);
            }
        }
    }

    std::cout << "Extracted descriptor layouts.\n";




    // Extract vertex attributes.

    // Extract parameters.

    // Name.

    // Type.

    // Bind position.




    return extract_info;
}
