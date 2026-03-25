#pragma once

#include "Axion/Common/Defines.h"
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
    enum class Level : uchar
    {
        Info,
        Warn,
        Error,
        None
    };

    /**
     * @brief Engine sub-modules used for categorizing log messages.
     */
    enum class Module : uchar
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

    /** @brief Convert engine module to string. */
    static const char* moduleToString( Module module );

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

#ifdef AXION_DEBUG

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
