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
static string const k_ansi_cc_reg_black  = "\e[0;30m";
static string const k_ansi_cc_reg_red    = "\e[0;31m";
static string const k_ansi_cc_reg_green  = "\e[0;32m";
static string const k_ansi_cc_reg_yellow = "\e[0;33m";
static string const k_ansi_cc_reg_blue   = "\e[0;34m";
static string const k_ansi_cc_reg_purple = "\e[0;35m";
static string const k_ansi_cc_reg_cyan   = "\e[0;36m";
static string const k_ansi_cc_reg_white  = "\e[0;37m";

static string const k_ansi_cc_bold_black  = "\e[1;30m";
static string const k_ansi_cc_bold_red    = "\e[1;31m";
static string const k_ansi_cc_bold_green  = "\e[1;32m";
static string const k_ansi_cc_bold_yellow = "\e[1;33m";
static string const k_ansi_cc_bold_blue   = "\e[1;34m";
static string const k_ansi_cc_bold_purple = "\e[1;35m";
static string const k_ansi_cc_bold_cyan   = "\e[1;36m";
static string const k_ansi_cc_bold_white  = "\e[1;37m";

static string const k_ansi_cc_reset = "\e[0m";

// Helper functions.
string get_type_prefix_str(Log_type type, bool ansi_colored, uint32_t& out_prefix_length)
{
    string iter_str{ to_string(s_mainloop_iteration.load()) };
    switch (type)
    {
        case TRACE:
        {
            string no_ansi_str{ "[trace](" + iter_str + ") :: " };
            out_prefix_length = no_ansi_str.size();
            if (ansi_colored)
                return (k_ansi_cc_bold_white + "[" +
                        k_ansi_cc_bold_cyan  + "trace" +
                        k_ansi_cc_bold_white + "](" + iter_str + ") :: " +
                        k_ansi_cc_reset);
            else
                return no_ansi_str;
        }

        case WARN:
        {
            string no_ansi_str{ "[warn](" + iter_str + ") :: " };
            out_prefix_length = no_ansi_str.size();
            if (ansi_colored)
                return (k_ansi_cc_bold_white  + "[" +
                        k_ansi_cc_bold_yellow + "warn" +
                        k_ansi_cc_bold_white  + "](" + iter_str + ") :: " +
                        k_ansi_cc_reset);
            else
                return no_ansi_str;
        }

        case ERROR:
        {
            string no_ansi_str{ "[error](" + iter_str + ") :: " };
            out_prefix_length = no_ansi_str.size();
            if (ansi_colored)
                return (k_ansi_cc_bold_white + "[" +
                        k_ansi_cc_bold_red   + "error" +
                        k_ansi_cc_bold_white + "](" + iter_str + ") :: " +
                        k_ansi_cc_reset);
            else
                return no_ansi_str;
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
    string prefix{ get_type_prefix_str(type, false, prefix_length) };

    // Cut up entry into column width.
    vector<string> rows;
    size_t i{ 0 };
    do
    {
        uint32_t remaining_columns{ k_num_columns };
        stringstream row;

        if (i == 0)
        {
            // Add prefix str.
            row << prefix;
            remaining_columns -= prefix_length;
        }
        else
        {
            // Add a small tab.
            static string const k_small_tab{ "   " };
            row << k_small_tab;
            remaining_columns -= k_small_tab.size();
        }

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
