#include "btlogger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>  // @NOTE: @TEMP: See `printe()`
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using std::array;
using std::atomic_uint32_t;
using std::atomic_uint64_t;
using std::min;
using std::stringstream;
using std::to_string;
using std::vector;


namespace BT::logger
{

static atomic_uint64_t s_mainloop_iteration{ 0 };

constexpr uint32_t k_num_columns{ 120 };
constexpr uint32_t k_num_columns_w_encoding{ k_num_columns + sizeof("\e[9;99x") * 4 };
constexpr uint32_t k_num_rows{ 16384 };  // @NOTE: Must be a power of 2.
static atomic_uint32_t s_next_entry{ 0 };
static atomic_uint32_t s_recording_head_entry{ 0 };  // For keeping track of log entries.

struct Row_entry
{
    Log_type log_type;
    char char_columns[k_num_columns_w_encoding];
};
static array<Row_entry, k_num_rows> s_all_entry_data;

static atomic_uint32_t s_print_mask{ ALL };

// ANSI color codes (https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124).
#define ANSI_CC_REG_BLACK    "\e[0;30m"
#define ANSI_CC_REG_RED      "\e[0;31m"
#define ANSI_CC_REG_GREEN    "\e[0;32m"
#define ANSI_CC_REG_YELLOW   "\e[0;33m"
#define ANSI_CC_REG_BLUE     "\e[0;34m"
#define ANSI_CC_REG_PURPLE   "\e[0;35m"
#define ANSI_CC_REG_CYAN     "\e[0;36m"
#define ANSI_CC_REG_WHITE    "\e[0;37m"

#define ANSI_CC_BOLD_BLACK   "\e[1;30m"
#define ANSI_CC_BOLD_RED     "\e[1;31m"
#define ANSI_CC_BOLD_GREEN   "\e[1;32m"
#define ANSI_CC_BOLD_YELLOW  "\e[1;33m"
#define ANSI_CC_BOLD_BLUE    "\e[1;34m"
#define ANSI_CC_BOLD_PURPLE  "\e[1;35m"
#define ANSI_CC_BOLD_CYAN    "\e[1;36m"
#define ANSI_CC_BOLD_WHITE   "\e[1;37m"

#define ANSI_CC_RESET        "\e[0m"

// Prefix string funcs.
#define PREFIX_STRING_PART1(__ansi_color_1, __ansi_color_2, __text)                                \
    __ansi_color_1 "[" __ansi_color_2 __text __ansi_color_1 "]("
#define PREFIX_STRING_PART2(__ansi_color_reset) ") :: " __ansi_color_reset

// Helper functions.
string get_type_prefix_str(Log_type type, bool ansi_colored, uint32_t& out_prefix_length)
{
    string iter_str{ to_string(s_mainloop_iteration.load()) };
    switch (type)
    {
        case TRACE:
        {
            constexpr char k_prefix_string_part1[]{ PREFIX_STRING_PART1("", "", "trace") };
            constexpr char k_prefix_string_part2[]{ PREFIX_STRING_PART2("") };
            out_prefix_length = sizeof(k_prefix_string_part1) - 1 + sizeof(k_prefix_string_part2) -
                                1 + iter_str.size();

            if (ansi_colored)
            {
                constexpr char k_colored_prefix_string_part1[]{
                    PREFIX_STRING_PART1(ANSI_CC_BOLD_WHITE, ANSI_CC_BOLD_CYAN, "trace")
                };
                constexpr char k_colored_prefix_string_part2[]{ PREFIX_STRING_PART2(
                    ANSI_CC_RESET) };

                return (k_colored_prefix_string_part1 + iter_str + k_colored_prefix_string_part2);
            }
            else
                return (k_prefix_string_part1 + iter_str + k_prefix_string_part2);
        }

        case WARN:
        {
            constexpr char k_prefix_string_part1[]{ PREFIX_STRING_PART1("", "", "warn ") };
            constexpr char k_prefix_string_part2[]{ PREFIX_STRING_PART2("") };
            out_prefix_length = sizeof(k_prefix_string_part1) - 1 + sizeof(k_prefix_string_part2) -
                                1 + iter_str.size();

            if (ansi_colored)
            {
                constexpr char k_colored_prefix_string_part1[]{
                    PREFIX_STRING_PART1(ANSI_CC_BOLD_WHITE, ANSI_CC_BOLD_YELLOW, "warn ")
                };
                constexpr char k_colored_prefix_string_part2[]{ PREFIX_STRING_PART2(
                    ANSI_CC_RESET) };

                return (k_colored_prefix_string_part1 + iter_str + k_colored_prefix_string_part2);
            }
            else
                return (k_prefix_string_part1 + iter_str + k_prefix_string_part2);
        }

        case ERROR:
        {
            constexpr char k_prefix_string_part1[]{ PREFIX_STRING_PART1("", "", "error") };
            constexpr char k_prefix_string_part2[]{ PREFIX_STRING_PART2("") };
            out_prefix_length = sizeof(k_prefix_string_part1) - 1 + sizeof(k_prefix_string_part2) -
                                1 + iter_str.size();

            if (ansi_colored)
            {
                constexpr char k_colored_prefix_string_part1[]{
                    PREFIX_STRING_PART1(ANSI_CC_BOLD_WHITE, ANSI_CC_BOLD_RED, "error")
                };
                constexpr char k_colored_prefix_string_part2[]{ PREFIX_STRING_PART2(
                    ANSI_CC_RESET) };

                return (k_colored_prefix_string_part1 + iter_str + k_colored_prefix_string_part2);
            }
            else
                return (k_prefix_string_part1 + iter_str + k_prefix_string_part2);
        }

        case INFO:
        {
            constexpr char k_prefix_string_part1[]{ PREFIX_STRING_PART1("", "", "info ") };
            constexpr char k_prefix_string_part2[]{ PREFIX_STRING_PART2("") };
            out_prefix_length = sizeof(k_prefix_string_part1) - 1 + sizeof(k_prefix_string_part2) -
                                1 + iter_str.size();

            if (ansi_colored)
            {
                constexpr char k_colored_prefix_string_part1[]{
                    PREFIX_STRING_PART1(ANSI_CC_BOLD_WHITE, ANSI_CC_BOLD_PURPLE, "info ")
                };
                constexpr char k_colored_prefix_string_part2[]{ PREFIX_STRING_PART2(
                    ANSI_CC_RESET) };

                return (k_colored_prefix_string_part1 + iter_str + k_colored_prefix_string_part2);
            }
            else
                return (k_prefix_string_part1 + iter_str + k_prefix_string_part2);
        }

        default:
            // Unsupported Log type (or is aggregate type which is unsupported also).
            assert(false);
            return "";
    }
}

bool check_show_type(Log_type type)
{
    return (s_print_mask & type);
}

uint32_t reserve_rows(uint32_t num_rows)
{
    return ((s_next_entry += num_rows) - num_rows);
}

}  // namespace BT::logger


void BT::logger::notify_start_new_mainloop_iteration()
{
    s_mainloop_iteration++;
}

void BT::logger::set_logging_print_mask(Log_type types)
{
    s_print_mask = types;
}

void BT::logger::printef(Log_type type, string format_str, ...)
{
    constexpr size_t k_str_max_length{ 1024 };
    char complete_str[k_str_max_length];

    va_list vag;
    va_start(vag, format_str);
    vsnprintf(complete_str, k_str_max_length, format_str.c_str(), vag);
    va_end(vag);

    printe(type, complete_str);
}

void BT::logger::printe(Log_type type, string entry)
{
    uint32_t prefix_length;
    string prefix{ get_type_prefix_str(type, true, prefix_length) };

    // Cut up entry into column width.
    vector<string> rows;
    size_t i{ 0 };
    do
    {
        uint32_t remaining_columns{ k_num_columns };
        stringstream row;

        // Add prefix.
        row << (i == 0 ? prefix : std::string(prefix_length, ' '));
        remaining_columns -= prefix_length;

        // Insert as much as possible.
        uint32_t insert_amount{ min(static_cast<uint32_t>(entry.size() - i),
                                    remaining_columns) };

        // Check for newline chars.
        bool has_newline_char{ false };
        if (size_t newline_pos = entry.find('\n', i); newline_pos != std::string::npos)
        {   // End early at the newline char.
            insert_amount = min(insert_amount, static_cast<uint32_t>(newline_pos - i));
            has_newline_char = true;
        }

        // Add requested substring amount.
        row << entry.substr(i, insert_amount);
        i += insert_amount + (has_newline_char ? 1 : 0);

        // Assert that somehow we didn't accidentally make too long of a string.
        // @NOTE: Use the length w/ encoding since we're comparing the string length w/ encoding.
        assert(row.str().size() <= k_num_columns_w_encoding);

        rows.emplace_back(row.str());
    } while (i < entry.size());

    // Copy strings into rows.
    uint32_t head_row{ reserve_rows(rows.size()) };
    for (auto& row : rows)
    {
        uint32_t current_row{ head_row % k_num_rows };
        s_all_entry_data[current_row].log_type = type;
        strncpy(s_all_entry_data[current_row].char_columns, row.c_str(), row.size());
        head_row++;
    }

    if (check_show_type(type))
    {
        // Print out to stdout.
        static std::mutex s_print_mutex;
        std::lock_guard<std::mutex> lock{ s_print_mutex };

        for (auto& row : rows)
        {
            // @NOTE: Idk why but fmt::println just doesn't really work when I'm compiling
            //   with clang-cl. I think that's what causes it to not work? But for now it'll
            //   have to be with iostream which is sloooow but in the future if you can figure
            //   out what to do to fix fmt, then do it and use it!  -Thea 2025/05/23
            // fmt::println("{}", row);

            // @TEMP: Hopefully.  ^^See above^^
            std::cout << row << std::endl;
        }
    }
}

tuple<uint32_t, uint32_t> BT::logger::get_head_and_tail()
{
    return { s_recording_head_entry.load(), s_next_entry.load() };
}

tuple<BT::logger::Log_type, char const*> BT::logger::read_log_entry(uint32_t row_idx)
{
    uint32_t actual_row_idx{ row_idx % k_num_rows };
    return { s_all_entry_data[actual_row_idx].log_type,
             s_all_entry_data[actual_row_idx].char_columns };
}

void BT::logger::clear_log_entries()
{
    s_recording_head_entry = s_next_entry.load();
}
