#pragma once

#include <chrono>


namespace BT
{

using high_res_time_t = std::chrono::high_resolution_clock::time_point;
using high_res_duration_t = std::chrono::high_resolution_clock::duration;

class Timer
{
public:
    void start_timer();
    float_t calc_delta_time();

private:
    bool m_started{ false };
    high_res_time_t m_prev_time;
};

}  // namespace BT
