#include "Axion/Common/Logging.h"

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
        case Level::Info:
            return "INFO";
        case Level::Warn:
            return "WARN";
        case Level::Error:
            return "ERROR";
        case Level::None:
            return "NONE";
        default:
            return "UNKNOWN";
    }
}

const char* Logger::levelColor( Level level ) {
    switch ( level )
    {
        case Level::Info:
            return "\033[32m"; // Green
        case Level::Warn:
            return "\033[33m"; // Yellow
        case Level::Error:
            return "\033[31m"; // Red
        default:
            return "\033[0m";
    }
}

const char* Logger::resetColor() {
    return "\033[0m";
}

const char* Logger::moduleToString( Module module ) {
    switch ( module )
    {
        case Module::Core:
            return "Core";
        case Module::GFX:
            return "GFX";
        case Module::Shader:
            return "GFX::Shader";
        case Module::RHI:
            return "GFX::RHI";
        case Module::Editor:
            return "Editor";
        default:
            return "Unknown";
    }
}

// ----------------- Logging -----------------

void Logger::log( Level level, Module module, const std::string& message ) {
    if ( level < instance()._logLevel )
        return;
    if ( module > instance()._logModule )
        return;

    std::lock_guard<std::mutex> lock( instance()._mtx );

    std::string timestamp = getTimestamp();
    std::string header    = fmt::format( "[AXION][{}][{}][{}] ",
                                      timestamp,
                                      levelToString( level ),
                                      moduleToString( module ) );

    std::string output;

    // If you still want multi-line indentation:
    if ( message.find( '\n' ) != std::string::npos )
    {
        std::istringstream stream( message );
        std::string        line;
        output = header + "\n";
        while ( std::getline( stream, line ) )
        {
            if ( !line.empty() )
                output += "    " + line + "\n"; // indent nicely
        }
    } else
    {
        output = header + message;
    }

    // Console output with color
    std::cout << levelColor( level ) << output << resetColor() << std::endl;

    // File output if enabled
    if ( instance()._logFile.is_open() )
        instance()._logFile << output << std::endl;
}

void Logger::log( Level level, Module module, const std::string& message, const char* file, int line, const char* func ) {
    if ( level < instance()._logLevel )
        return;
    if ( module > instance()._logModule )
        return;

    std::lock_guard<std::mutex> lock( instance()._mtx );

    std::string output = std::format( "[AXION][{}][{}][{}] {} ({}:{} {})",
                                      getTimestamp(),
                                      levelToString( level ),
                                      moduleToString( module ),
                                      message,
                                      file,
                                      line,
                                      func );

    std::cout << levelColor( level ) << output << resetColor() << std::endl;

    if ( instance()._logFile.is_open() )
        instance()._logFile << output << std::endl;
}

void Logger::flush() {
    std::scoped_lock lock( instance()._mtx );
    if ( instance()._logFile.is_open() )
        instance()._logFile.flush();
}
AXION_NAMESPACE_END
