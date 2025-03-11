#include "log.h"

#include <stdarg.h>

namespace wyre {

/* CLI Colors */
constexpr char const* const RESET_C = "\x1b[0m";
constexpr char const* const INFO_C = "\x1b[92m";
constexpr char const* const WARNING_C = "\x1b[93m";
constexpr char const* const CRITICAL_C = "\x1b[91m";
constexpr char const* const MUTED_C = "\x1b[2m";
constexpr char const* const BOLD_C = "\x1b[1m";

/* Get command line color based on log level */
inline static std::string_view level_as_color(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::INFO: return INFO_C;
        case LogLevel::WARNING: return WARNING_C;
        case LogLevel::CRITICAL: return CRITICAL_C;
        default: return "";
    }
}

/* Convert level of severity to a string */
inline static std::string_view level_as_string(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::INFO: return "info";
        case LogLevel::WARNING: return "warning";
        case LogLevel::CRITICAL: return "critical";
        default: return " unknown";
    }
}

/* Convert logging group to a string */
inline static std::string_view group_as_string(const LogGroup group) noexcept {
    switch (group) {
        case LogGroup::PROGRAM: return "program";
        case LogGroup::SYSTEM: return "system";
        case LogGroup::GRAPHICS_API: return "graphics";
        default: return "unknown";
    }
}

// TODO: maybe use "fmt" library? <https://github.com/fmtlib/fmt>

void Logger::log_inner(const LogGroup group, const LogLevel level, const std::string_view msg) {
    /* Get the current time */
    time_t now = time(0);
    tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestamp[80];
    strftime(timestamp, sizeof(timestamp), "(%d-%m-%Y|%H:%M:%S)", &timeinfo);

    /* Format the log entry */
    std::ostringstream entry;
    const std::string_view level_str = level_as_string(level);
    const std::string_view group_str = group_as_string(group);

    /* Output into 'cout' */
    std::cout << MUTED_C << timestamp << " " << RESET_C;
    std::cout << BOLD_C << level_as_color(level);
    std::cout << level_str << RESET_C << ": " << MUTED_C << "[" << group_str << "] " << RESET_C << msg << std::endl;
    std::cout << RESET_C;

    /* Output to the log file */
    entry << timestamp << ": " << level_str << ": [" << group_str << "] " << msg << std::endl;

    if (fout.is_open()) {
        fout << entry.str();
        fout.flush();
    }
}

void Logger::log(const LogGroup group, const LogLevel level, const std::string_view fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;
    std::string str;
    va_list ap;
    for (;;) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char*)str.data(), size, fmt.data(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    log_inner(group, level, str);
}

void Logger::info(const std::string_view fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;
    std::string str;
    va_list ap;
    for (;;) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char*)str.data(), size, fmt.data(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    log_inner(LogGroup::PROGRAM, LogLevel::INFO, str);
}

void Logger::warn(const std::string_view fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;
    std::string str;
    va_list ap;
    for (;;) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char*)str.data(), size, fmt.data(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    log_inner(LogGroup::PROGRAM, LogLevel::WARNING, str);
}

void Logger::error(const std::string_view fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;
    std::string str;
    va_list ap;
    for (;;) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char*)str.data(), size, fmt.data(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    log_inner(LogGroup::PROGRAM, LogLevel::CRITICAL, str);
}

}  // namespace wyre
