#include "btdatecheck.h"

#include <chrono>
#include <cstdint>
#include <stdexcept>


void BT::date_deadline(int32_t year, uint32_t month, uint32_t day)
{
    std::chrono::year_month_day marked_date{
        std::chrono::year{ year },
        std::chrono::month{ month },
        std::chrono::day{ day }
    };

    auto today{ std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
    std::chrono::year_month_day current_date{ today };

    if (current_date > marked_date)
        throw std::runtime_error("Date deadline has passed.");
}
