#pragma once

#include <cstdint>


namespace BT
{

/// Checks whether the date has passed the marked deadline.
/// If so, throws.
/// @NOTE: this is meant to be for debug.  -Thea 2026/03/08
void date_deadline(int32_t year, uint32_t month, uint32_t day);

}  // namespace BT
