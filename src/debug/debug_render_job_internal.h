#pragma once

#include <cmath>
#include <cstddef>


namespace TXP
{
namespace debug
{

/// Custom vertex information for drawing debug lines.
struct Debug_line_vertex
{
    float_t pos_x;
    float_t pos_y;
    float_t pos_z;
    float_t color_r;
    float_t color_g;
    float_t color_b;
    float_t color_a;

    float_t* position()
    {
        return &pos_x;
    }

    float_t* color()
    {
        return &color_r;
    }
};

/// Updates times and sorts lines.
void tick_debug_render_jobs(float_t delta_time);

/// Calculates the number of debug lines to draw.
size_t calc_num_debug_lines();

/// Writes debug line information to a buffer in memory.
void write_debug_line_mem(void* dest);

} // namespace debug
} // namespace TXP
