#pragma once

#include <cmath>
#include <cstddef>


namespace TXP
{
namespace debug
{

/// Updates times and sorts lines.
void tick_debug_render_jobs(float_t delta_time);

/// Calculates the required size for the destination buffer to be able to hold debug line
/// information.
size_t calc_debug_line_mem_size();

/// Writes debug line information to a buffer in memory.
void write_debug_line_mem(void* dest);

} // namespace debug
} // namespace TXP
