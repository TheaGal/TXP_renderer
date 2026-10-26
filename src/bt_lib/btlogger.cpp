#include "btlogger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>  // @NOTE: @TEMP: See `printe()`
#include <mutex>
#include <sstream>
#include <stdexcept>
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

constexpr uint32_t k_num_columns{ 128 };
constexpr uint32_t k_num_columns_w_encoding{ k_num_columns + sizeof("\e[9;99x") * 4 };

constexpr uint32_t k_log_buffer_size{ k_num_columns * k_num_rows };
static_assert((k_log_buffer_size & (k_log_buffer_size - 1)) == 0, "ensure log buffer size is pow2");

static array<char, k_log_buffer_size> s_log_buffer{};  // zerod out by default

constexpr char k_reserved_etx_char{ 3 };
constexpr char k_reserved_empty_buffer_char{ 127 };

static atomic_uint64_t s_next_head{ 0 };
static atomic_uint64_t s_log_base_head{ 0 };  // For keeping track of logs.

// struct Row_entry
// {
//     Log_type log_type;
//     char char_columns[k_num_columns_w_encoding];
// };
// static array<Row_entry, k_num_rows> s_all_entry_data;

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
string get_type_prefix_str(Log_type type,
                           string const& iter_str,
                           bool ansi_colored,
                           uint32_t& out_prefix_length)
{
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

char& log_buffer_at(uint64_t const pos)
{
    uint32_t idx = (pos & (k_log_buffer_size - 1));
    return s_log_buffer[idx];
}

uint64_t reserve_bytes(uint32_t const num_bytes)
{
    assert(num_bytes > 0);
    assert(num_bytes < k_log_buffer_size);

    // Find new head.
    uint64_t original_head = s_next_head.load();
    uint64_t real_head;
    uint64_t next_head;
    do
    {
        real_head = original_head;
        next_head = (real_head + num_bytes);

        if ((real_head & (k_log_buffer_size - 1)) >= (next_head & (k_log_buffer_size - 1)))
        {
            // Wrap around.
            real_head += (k_log_buffer_size - (real_head & (k_log_buffer_size - 1)));
            next_head = (real_head + num_bytes);
            assert((real_head & (k_log_buffer_size - 1)) == 0);  // ensure modulos to 0
        }

        if (real_head < original_head)
            throw std::runtime_error(
                "wraparound. did you run this program for over 20k years?? holy shit");
    } while (!s_next_head.compare_exchange_weak(original_head, next_head));

    // Fill in difference of original and real head.
    for (uint64_t i = original_head; i != real_head; i++)
    {
        log_buffer_at(i) = k_reserved_empty_buffer_char;
    }

    return real_head;
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
#define DO_RESERVED_CHAR_SANITY_CHECK 1
#if DO_RESERVED_CHAR_SANITY_CHECK
    // Sanity check to make sure entry doesn't contain reserved char.
    for (char c : entry)
        if (c == k_reserved_empty_buffer_char)
            throw std::runtime_error("entry cannot have reserved char ASCII 127 (DELETE char)");
#endif // DO_RESERVED_CHAR_SANITY_CHECK

    uint64_t const mainloop_iteration{ s_mainloop_iteration.load() };

    // Copy data into buffer.
    {
        uint64_t write_pos{ reserve_bytes(sizeof(uint32_t) + sizeof(uint64_t) + entry.length() +
                                          sizeof(uint32_t) + 1) };

        // Log type.
        reinterpret_cast<uint32_t&>(log_buffer_at(write_pos)) = type;
        write_pos += sizeof(uint32_t);

        // Mainloop iteration.
        reinterpret_cast<uint64_t&>(log_buffer_at(write_pos)) = mainloop_iteration;
        write_pos += sizeof(uint64_t);

        // Actual string.
        strncpy(&log_buffer_at(write_pos), entry.c_str(), entry.length());
        write_pos += entry.length();

        // String length again (for reading backwards).
        reinterpret_cast<uint32_t&>(log_buffer_at(write_pos)) = entry.length();
        write_pos += sizeof(uint32_t);

        // Add reserved ETX.
        log_buffer_at(write_pos) = k_reserved_etx_char;
        write_pos++;
    }

    // Print to stdout console.
    if (check_show_type(type))
    {
        uint32_t prefix_length;
        string const prefix{
            get_type_prefix_str(type, to_string(mainloop_iteration), true, prefix_length)
        };

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
            // @NOTE: Use the length w/ encoding since we're comparing the string length w/
            // encoding.
            assert(row.str().size() <= k_num_columns_w_encoding);

            rows.emplace_back(row.str());
        } while (i < entry.size());

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

auto BT::logger::read_all_log_entries(uint32_t& out_start_row_idx)
    -> array<tuple<Log_type, uint64_t, string_view>, k_num_rows>
{
    array<tuple<Log_type, uint64_t, string_view>, k_num_rows> all_logs;

    uint64_t read_pos{ s_next_head.load() };

    uint64_t const read_head{ [read_pos]() {
        uint64_t head{ s_log_base_head.load() };
        if (read_pos >= k_log_buffer_size)
        {
            head = std::max(head, read_pos - (k_log_buffer_size - 2));
            while (log_buffer_at(head - 1) != k_reserved_etx_char)
                head++;
        }
        return head;
    }() };

    int32_t idx;
    for (idx = static_cast<int32_t>(k_num_rows) - 1; idx >= 0; idx--)
    {
        // Check whether finished reading all available logs.
        if (read_head == read_pos)
            break;

        // Check for wraparound.
        while (log_buffer_at(read_pos - 1) == k_reserved_empty_buffer_char)
            read_pos--;

        // Ensure there is ETX.
        read_pos--;
        char const jojojojojo{ log_buffer_at(read_pos) };
        assert(jojojojojo == k_reserved_etx_char);

        // Read str length.
        read_pos -= sizeof(uint32_t);
        uint32_t const str_length = reinterpret_cast<uint32_t&>(log_buffer_at(read_pos));

        // Read string.
        read_pos -= str_length;
        string_view const str_slice(&log_buffer_at(read_pos), static_cast<size_t>(str_length));

        // Read mainloop iteration.
        read_pos -= sizeof(uint64_t);
        uint64_t const mainloop_iteration = reinterpret_cast<uint64_t&>(log_buffer_at(read_pos));

        // Read log type.
        read_pos -= sizeof(uint32_t);
        Log_type type{ reinterpret_cast<uint32_t&>(log_buffer_at(read_pos)) };

        // Record entry.
        all_logs[idx] = { type, mainloop_iteration, str_slice };
    }

    out_start_row_idx = idx + 1;
    return all_logs;
}

void BT::logger::clear_log_entries()
{
    s_log_base_head.store(s_next_head.load());
}
