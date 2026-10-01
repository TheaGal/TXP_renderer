#pragma once

#include <cmath>
#include <string>


namespace TXP
{
namespace debug
{

/// Emplaces a piece of data into a data map.
void emplace_data_point(std::string const& data_label, float_t const data_point);

} // namespace debug
} // namespace TXP
