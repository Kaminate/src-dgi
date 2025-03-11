#pragma once

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace wyre {

/* Logging levels of severity. */
enum class LogLevel {
    /* Any info that may be useful during debugging. */
    INFO,
    /* Any info about failures that don't cause the program to exit. */
    WARNING,
    /* Any info about failures that cause the program to exit. */
    CRITICAL
};

/* Logging groups. */
enum class LogGroup {
    /* Any info related to the program in general. (default group) */
    PROGRAM,
    /* Any info related to the OS, windowing, input, files. */
    SYSTEM,
    /* Any info related to graphics APIs. */
    GRAPHICS_API
};

/**
 * @brief Central class for logging.
 */
class Logger {
    friend class WyreEngine;

    /* Standard output logging level. */
    LogLevel cout_level = LogLevel::WARNING;

    /* Output logging file filestream. */
    std::ofstream fout;

    Logger(const std::string_view filename, LogLevel log_level) : fout(filename.data(), std::ios::app), cout_level(log_level) {
        if (not fout.is_open()) {
            std::cerr << "[logger] failed to open log file!" << std::endl;
            return;
        }
        fout << "-------------------------------- session --------------------------------" << std::endl;
    }
    ~Logger() { fout.close(); }

    /**
     * @brief Log a message to the standard output & a logging file.
     *
     * @param group The logging group the message belongs to.
     * @param level The level of severity of the message.
     * @param msg The message.
     */
    void log_inner(const LogGroup group, const LogLevel level, const std::string_view msg);

   public:
    /**
     * @brief Log a message to the standard output & a logging file.
     *
     * @param group The logging group the message belongs to.
     * @param level The level of severity of the message.
     * @param fmt The string format of the message.
     */
    void log(const LogGroup group, const LogLevel level, const std::string_view fmt, ...);
    
    /**
     * @brief Print info message to the standard output. (useful for debugging)
     *
     * @param fmt The string format of the message.
     */
    void info(const std::string_view fmt, ...);
    
    /**
     * @brief Print warning message to the standard output. (useful for debugging)
     *
     * @param fmt The string format of the message.
     */
    void warn(const std::string_view fmt, ...);
    
    /**
     * @brief Print error message to the standard output. (useful for debugging)
     *
     * @param fmt The string format of the message.
     */
    void error(const std::string_view fmt, ...);
};

}  // namespace wyre
