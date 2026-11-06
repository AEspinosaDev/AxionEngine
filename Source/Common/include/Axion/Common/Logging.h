#pragma once

#include "Axion/Common/Common.h"
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

AXION_NAMESPACE_BEGIN

/**
 * @brief Lightweight thread-safe logger for Raiko engine.
 *
 * Supports logging to console and optional file output.
 * Messages can be color-coded based on severity level.
 */
class Logger
{
public:
    /**
     * @brief Log severity levels.
     */
    enum class Level : byte
    {
        Info,
        Warn,
        Error,
        None
    };

    /**
     * @brief Engine sub-modules used for categorizing log messages.
     */
    enum class Module : byte
    {
        Editor, /**< Editor Application */
        Core,   /**< Core engine systems */
        GFX,    /**< Graphics system */
        RHI,    /**< Rendering Hardware Interface Subsystem*/
        Shader, /**< Shader Compiling*/
        Common, /**< Common*/
        All
    };

    /**
     * @brief Initialize the logger.
     *
     * @param level Minimum log level to display.
     * @param file Optional file path to log to.
     * @param module Maximun module to display logging.
     * @param truncate If true, clears the file on init; otherwise, appends.
     */
    static void init( Level level = Level::Info, const std::string& file = "", Module module = Module::All, bool truncate = true );

    /**
     * @brief Shutdown the logger and close any file streams.
     */
    static void shutdown();

    /**
     * @brief Log a message without context.
     *
     * @param level Severity level.
     * @param module Engine module.
     * @param message Formatted message string.
     */
    static void log( Level level, Module module, const std::string& message );

    /**
     * @brief Log a message with file, line, and function context.
     *
     * @param level Severity level.
     * @param module Engine module.
     * @param message Formatted message string.
     * @param file Source file of log call.
     * @param line Line number of log call.
     * @param func Function name of log call.
     */
    static void log( Level level, Module module, const std::string& message, const char* file, int line, const char* func );

    /**
     * @brief Flush the logger buffer to the console/file.
     */
    static void flush();

    /**
     * @brief Set the runtime log level.
     *
     * @param level Minimum severity to log.
     */
    static void setLogLevel( Level level );

    /** @brief Convert engine module to string. */
    static const char* moduleToString( Module module );

private:
    Logger()                           = default;
    ~Logger()                          = default;
    Logger( const Logger& )            = delete;
    Logger& operator=( const Logger& ) = delete;

    /**
     * @brief Get the singleton instance of the logger.
     * @return Reference to the logger instance.
     */
    static Logger& instance();

    /** @brief Get a timestamp string for log entries. */
    static std::string getTimestamp();

    /** @brief Convert log level to string. */
    static const char* levelToString( Level level );

    /** @brief Get ANSI color code for the log level. */
    static const char* levelColor( Level level );

    /** @brief Get ANSI color code for the log module. */
    static const char* moduleColor( Module module );

    /** @brief Reset ANSI color. */
    static const char* resetColor();

private:
    Level         _logLevel  = Level::Info;
    Module        _logModule = Module::All;
    std::ofstream _logFile;
    std::mutex    _mtx;
};

AXION_NAMESPACE_END

// -------------------------------------------------
// Macros for convenience
// -------------------------------------------------

// Tools run from a console in any configuration: their logs are their output
#if defined( AXION_DEBUG ) || defined( AXION_TOOLS_BUILD )

#define AXION_LOG_INFO( module, fmt_str, ... ) \
    Axion::Logger::log(                        \
        Axion::Logger::Level::Info,            \
        module,                                \
        fmt::format( fmt_str, ##__VA_ARGS__ ) )
#define AXION_LOG_WARN( module, fmt_str, ... ) \
    Axion::Logger::log(                        \
        Axion::Logger::Level::Warn,            \
        module,                                \
        fmt::format( fmt_str, ##__VA_ARGS__ ), \
        __FILE__,                              \
        __LINE__,                              \
        __func__ )

#define AXION_LOG_ERROR( module, fmt_str, ... ) \
    Axion::Logger::log(                         \
        Axion::Logger::Level::Error,            \
        module,                                 \
        fmt::format( fmt_str, ##__VA_ARGS__ ),  \
        __FILE__,                               \
        __LINE__,                               \
        __func__ )

#define AXION_LOG_ASSERT( cond, module, msg, ... )         \
    do                                                     \
    {                                                      \
        if ( !( cond ) )                                   \
        {                                                  \
            AXION_LOG_ERROR( module, msg, ##__VA_ARGS__ ); \
            AXION_DEBUG_BREAK();                            \
            Axion::Logger::flush();                        \
            std::abort();                                  \
        }                                                  \
    } while ( 0 )

#define AXION_LOG_WARN_ONCE( module, fmt, ... )         \
    do                                                  \
    {                                                   \
        static bool _axion_log_once_guard = false;      \
        if ( !_axion_log_once_guard )                   \
        {                                               \
            AXION_LOG_WARN( module, fmt, __VA_ARGS__ ); \
            _axion_log_once_guard = true;               \
        }                                               \
    } while ( 0 )

#else

#define AXION_LOG_INFO( ... ) ( (void)0 )
#define AXION_LOG_WARN( ... ) ( (void)0 )
#define AXION_LOG_ERROR( ... ) ( (void)0 )
#define AXION_LOG_ASSERT( cond, module, msg, ... ) ( (void)0 )
#define AXION_LOG_WARN_ONCE( ... ) ( (void)0 )

#endif

// -------------------------------------------------
// Fatal errors: defined in every build configuration
// -------------------------------------------------

/**
 * @brief Prints a fatal error straight to the console (stderr). For the few errors that must be seen in Release too.
 * It doesn't go through Logger, so it works before Logger::init and ignores the log level and module filters.
 * It doesn't end the application: that's up to the caller.
 */
#define AXION_LOG_FATAL_ERROR( module, fmt_str, ... )      \
    fmt::println( stderr,                                  \
                  "[AXION][FATAL][{}] {}",                 \
                  Axion::Logger::moduleToString( module ), \
                  fmt::format( fmt_str, ##__VA_ARGS__ ) )

/**
 * @brief Shows a fatal error in a dialog and waits until the user closes it.
 * An application started with a double click closes its console on exit, so the console message alone would be lost.
 * It doesn't end the application: that's up to the caller.
 */
#ifdef _WIN32
#define AXION_FATAL_ERROR_POPUP_WINDOW( fmt_str, ... )            \
    ::MessageBoxA( nullptr,                                       \
                   fmt::format( fmt_str, ##__VA_ARGS__ ).c_str(), \
                   "Axion Engine",                                \
                   MB_OK | MB_ICONERROR )
#else
#define AXION_FATAL_ERROR_POPUP_WINDOW( ... ) ( (void)0 )
#endif
