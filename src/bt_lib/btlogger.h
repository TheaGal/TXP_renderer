#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <tuple>

using std::array;
using std::string;
using std::string_view;
using std::tuple;


namespace BT
{
namespace logger
{

constexpr uint32_t k_num_rows{ 4096 };
static_assert((k_num_rows & (k_num_rows - 1)) == 0, "Must be a power of 2.");

enum Log_type : uint32_t
{
    NONE  = 0b0000,
    ALL   = 0xffffffff,

    TRACE = 0b0001,
    WARN  = 0b0010,
    ERROR = 0b0100,
    INFO  = 0b1000,
};

void notify_start_new_mainloop_iteration();
void set_logging_print_mask(Log_type types);
void printef(Log_type type, string format_str, ...);
void printe(Log_type type, string entry);

array<tuple<Log_type, uint64_t, string_view>, k_num_rows> read_all_log_entries(
    uint32_t& out_start_row_idx);
void clear_log_entries();

}  // namespace logger
}  // namespace BT


/// Some helper macros for logging.
#ifdef BT_TRACE
    #error Macro `BT_TRACE` already defined.
#else
    #define BT_TRACE(x)  BT::logger::printe(BT::logger::TRACE, x)
#endif  // BT_TRACE

#ifdef BT_WARN
    #error Macro `BT_WARN` already defined.
#else
    #define BT_WARN(x)  BT::logger::printe(BT::logger::WARN, x)
#endif  // BT_WARN

#ifdef BT_ERROR
    #error Macro `BT_ERROR` already defined.
#else
    #define BT_ERROR(x)  BT::logger::printe(BT::logger::ERROR, x)
#endif  // BT_ERROR

#ifdef BT_INFO
    #error Macro `BT_INFO` already defined.
#else
    #define BT_INFO(x)  BT::logger::printe(BT::logger::INFO, x)
#endif  // BT_INFO

#ifdef BT_TRACEF
    #error Macro `BT_TRACEF` already defined.
#else
    #define BT_TRACEF(x, ...)  BT::logger::printef(BT::logger::TRACE, x, __VA_ARGS__)
#endif  // BT_TRACEF

#ifdef BT_WARNF
    #error Macro `BT_WARNF` already defined.
#else
    #define BT_WARNF(x, ...)  BT::logger::printef(BT::logger::WARN, x, __VA_ARGS__)
#endif  // BT_WARNF

#ifdef BT_ERRORF
    #error Macro `BT_ERRORF` already defined.
#else
    #define BT_ERRORF(x, ...)  BT::logger::printef(BT::logger::ERROR, x, __VA_ARGS__)
#endif  // BT_ERRORF

#ifdef BT_INFOF
    #error Macro `BT_INFOF` already defined.
#else
    #define BT_INFOF(x, ...)  BT::logger::printef(BT::logger::INFO, x, __VA_ARGS__)
#endif  // BT_INFOF

#ifdef BT_CHK_RETVAL
    #error Macro `BT_CHK_RETVAL` already defined.
#else
#define BT_CHK_RETVAL(__checkstmnt, __expectedval)                                                 \
    do                                                                                             \
    {                                                                                              \
        auto const actual_val{ (__checkstmnt) };                                                   \
        auto const expected_val{ (__expectedval) };                                                \
        if (actual_val != expected_val)                                                            \
        {                                                                                          \
            BT_ERROR(std::string(__FILE__ "(") + std::to_string(__LINE__) +                        \
                     std::string("): statement `" #__checkstmnt "` expected: ") +                  \
                     std::to_string(expected_val) + "  actual: " + std::to_string(actual_val));    \
            assert(false);                                                                         \
        }                                                                                          \
    } while (0)
#endif // BT_CHK_RETVAL
