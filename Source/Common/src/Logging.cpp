#include "Axion/Common/Logging.h"
#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <sstream>

AXION_NAMESPACE_BEGIN

// ----------------- Initialization -----------------
void Logger::init( Level level, const std::string& file, Module module, bool truncate ) {
    auto& logger      = instance();
    logger._logLevel  = level;
    logger._logModule = module;
    if ( !file.empty() )
    {
        if ( truncate )
            logger._logFile.open( file, std::ios::out | std::ios::trunc );
        else
            logger._logFile.open( file, std::ios::out | std::ios::app );
    }
    logger._logFile << "---------------------------------------------------------------------------------- " << std::endl;
    logger._logFile << "----------------------- NEW EXECUTION " << "[" << getTimestamp() << "] ---------------------- " << std::endl;
    logger._logFile << "---------------------------------------------------------------------------------- " << std::endl;
    logger._logFile.flush();
}

void Logger::shutdown() {
    auto& logger = instance();
    if ( logger._logFile.is_open() )
        logger._logFile.close();
}

void Logger::setLogLevel( Level level ) {
    instance()._logLevel = level;
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

// ----------------- Helpers -----------------
std::string Logger::getTimestamp() {
    auto        now = std::chrono::system_clock::now();
    std::time_t t   = std::chrono::system_clock::to_time_t( now );
    std::tm     tm {};
#ifdef _WIN32
    localtime_s( &tm, &t );
#else
    localtime_r( &t, &tm );
#endif
    char buffer[20];
    std::strftime( buffer, sizeof( buffer ), "%Y-%m-%d %H:%M:%S", &tm );
    return buffer;
}

const char* Logger::levelToString( Level level ) {
    switch ( level )
    {
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        case Level::None:  return "NONE";
        default:           return "UNKNOWN";
    }
}

const char* Logger::moduleToString( Module module ) {
    switch ( module )
    {
        case Module::Core:   return "Core";
        case Module::GFX:    return "GFX";
        case Module::Shader: return "GFX::Shader";
        case Module::RHI:    return "GFX::RHI";
        case Module::Editor: return "Editor";
        default:             return "Unknown";
    }
}

const char* Logger::levelColor( Level level ) {
    switch ( level )
    {
        case Level::Info:  return "\033[32m"; // Green
        case Level::Warn:  return "\033[33m"; // Yellow
        case Level::Error: return "\033[31m"; // Red
        default:           return "\033[0m";
    }
}

// Helper interno para color por módulo
const char* Logger::moduleColor( Module module ) {
    switch ( module )
    {
        case Module::Core:   return "\033[36m"; // Cyan
        case Module::GFX:    return "\033[35m"; // Magenta
        case Module::Shader: return "\033[35m"; // Bright Green
        case Module::RHI:    return "\033[35m"; // Bright Blue
        case Module::Editor: return "\033[97m"; // Bright White
        default:             return "\033[37m"; // Gray
    }
}

const char* Logger::resetColor() {
    return "\033[0m";
}

// ----------------- Logging -----------------

void Logger::log( Level level, Module module, const std::string& message ) {
    if ( level < instance()._logLevel ) return;
    if ( module > instance()._logModule ) return;

    std::lock_guard<std::mutex> lock( instance()._mtx );

    std::string timestamp = getTimestamp();

    // 1. Header para ARCHIVO (Texto plano, sin códigos de color)
    std::string headerFile = std::format( "[AXION][{}][{}][{}] ", 
                                          timestamp, 
                                          levelToString( level ), 
                                          moduleToString( module ) );

    // 2. Header para CONSOLA (Inyección de color solo en el módulo)
    // Explicación: cout << levelColor(...) aplica el color global (ej: verde).
    // Aquí interrumpimos ese verde (resetColor), ponemos el color del módulo, 
    // escribimos el módulo, reseteamos, y restauramos el verde (levelColor) para que el mensaje siga verde.
    std::string headerConsole = std::format( "[AXION][{}][{}][{}{}{}{}{}] ",
                                             timestamp,
                                             levelToString( level ),
                                             resetColor(),             // Stop Level Color
                                             moduleColor( module ),    // Start Module Color
                                             moduleToString( module ), // Module Text
                                             resetColor(),             // Stop Module Color
                                             levelColor( level ) );    // Restart Level Color

    std::string outputFile;
    std::string outputConsole;

    if ( message.find( '\n' ) != std::string::npos )
    {
        std::istringstream stream( message );
        std::string        line;
        
        outputFile    = headerFile + "\n";
        outputConsole = headerConsole + "\n";
        
        while ( std::getline( stream, line ) )
        {
            if ( !line.empty() ) {
                outputFile    += "    " + line + "\n";
                outputConsole += "    " + line + "\n";
            }
        }
    } else
    {
        outputFile    = headerFile + message;
        outputConsole = headerConsole + message;
    }

    // Console output (con color principal del nivel)
    std::cout << levelColor( level ) << outputConsole << resetColor() << std::endl;

    // File output (limpio)
    if ( instance()._logFile.is_open() )
        instance()._logFile << outputFile << std::endl;
}

void Logger::log( Level level, Module module, const std::string& message, const char* file, int line, const char* func ) {
    if ( level < instance()._logLevel ) return;
    if ( module > instance()._logModule ) return;

    std::lock_guard<std::mutex> lock( instance()._mtx );

    std::string timestamp = getTimestamp();

    // 1. Header Archivo
    std::string headerFile = std::format( "[AXION][{}][{}][{}] ", 
                                          timestamp, 
                                          levelToString( level ), 
                                          moduleToString( module ) );

    // 2. Header Consola (Misma técnica de inyección)
    std::string headerConsole = std::format( "[AXION][{}][{}][{}{}{}{}{}] ",
                                             timestamp,
                                             levelToString( level ),
                                             resetColor(),
                                             moduleColor( module ),
                                             moduleToString( module ),
                                             resetColor(),
                                             levelColor( level ) );

    std::string debugInfo = std::format( "{} ({}:{} {})", message, file, line, func );

    std::string outputFile    = headerFile + debugInfo;
    std::string outputConsole = headerConsole + debugInfo;

    std::cout << levelColor( level ) << outputConsole << resetColor() << std::endl;

    if ( instance()._logFile.is_open() )
        instance()._logFile << outputFile << std::endl;
}

void Logger::flush() {
    std::scoped_lock lock( instance()._mtx );
    if ( instance()._logFile.is_open() )
        instance()._logFile.flush();
}

AXION_NAMESPACE_END