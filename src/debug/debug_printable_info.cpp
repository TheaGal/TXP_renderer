// clang-format off
#include "txp_renderer/debug/debug_printable_info.h"
#include "debug_printable_info_internal.h"
// clang-format on

#include <cmath>
#include <map>
#include <sstream>
#include <string>


namespace TXP
{
namespace
{

std::map<std::string, float_t> g_data_points;

} // namespace


void debug::emplace_data_point(std::string const& data_label, float_t const data_point)
{
    g_data_points[data_label] = data_point;
}

std::string debug::get_printable_info_data_as_str()
{
    std::ostringstream oss;
    for (auto const& [data_label, data_point] : g_data_points)
        oss << data_label << " : " << data_point << "\n";

    return oss.str();
}

} // namespace TXP
