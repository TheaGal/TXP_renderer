#pragma once

#include <cstdint>
#include <string>


namespace TXP
{
namespace Shader_Support
{

/// Fetches information from compute shader reflection data.
void fetch_compute_shader_info(std::string const& shader_name,
                               std::string& out_compute_entry_point_name,
                               uint32_t& out_thread_group_size_x,
                               uint32_t& out_thread_group_size_y,
                               uint32_t& out_thread_group_size_z);

/// Fetches information from graphics shader reflection data.
void fetch_graphics_shader_info(std::string const& shader_name,
                                std::string& out_vertex_entry_point_name,
                                std::string& out_fragment_entry_point_name);

}  // namespace Shader_Support
}  // namespace TXP
