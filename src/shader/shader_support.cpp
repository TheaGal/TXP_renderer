#include "shader_support.h"

#include "shader_creation/shader_creation.h"

#include <cstdint>
#include <string>


namespace TXP
{

void Shader_Support::fetch_compute_shader_info(std::string const& shader_name,
                                               std::string& out_compute_entry_point_name,
                                               uint32_t& out_thread_group_size_x,
                                               uint32_t& out_thread_group_size_y,
                                               uint32_t& out_thread_group_size_z)
{
    auto refl_data = Shader_Creation::read_slang_reflection(shader_name);

    if (refl_data.entryPoints.size() != 1 ||
        refl_data.entryPoints.front().stage != "compute" ||
        refl_data.entryPoints.front().threadGroupSize.size() != 3 ||
        refl_data.entryPoints.front().threadGroupSize[0] <= 0 ||
        refl_data.entryPoints.front().threadGroupSize[1] <= 0 ||
        refl_data.entryPoints.front().threadGroupSize[2] <= 0)
        throw std::runtime_error("Malformed shader data.");

    out_compute_entry_point_name = refl_data.entryPoints.front().name;

    out_thread_group_size_x = refl_data.entryPoints.front().threadGroupSize[0];
    out_thread_group_size_y = refl_data.entryPoints.front().threadGroupSize[1];
    out_thread_group_size_z = refl_data.entryPoints.front().threadGroupSize[2];
}

void Shader_Support::fetch_graphics_shader_info(std::string const& shader_name,
                                                std::string& out_vertex_entry_point_name,
                                                std::string& out_fragment_entry_point_name)
{
    auto refl_data = Shader_Creation::read_slang_reflection(shader_name);

    if (refl_data.entryPoints.size() != 2 ||
        refl_data.entryPoints[0].stage != "vertex" ||
        refl_data.entryPoints[1].stage != "fragment")
        throw std::runtime_error("Malformed shader data.");

    out_vertex_entry_point_name = refl_data.entryPoints[0].name;
    out_fragment_entry_point_name = refl_data.entryPoints[1].name;
}

}  // namespace TXP
