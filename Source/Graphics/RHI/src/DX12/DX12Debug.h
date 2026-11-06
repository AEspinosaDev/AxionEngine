#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "DX12Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI::Debug {

inline std::string getLastDXGIMessage() {
    ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
    if ( FAILED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &dxgiInfoQueue ) ) ) )
        return {};

    std::stringstream ss;
    for ( UINT64 i = 0; i < dxgiInfoQueue->GetNumStoredMessages( DXGI_DEBUG_ALL ); ++i )
    {
        SIZE_T messageLength = 0;
        dxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, nullptr, &messageLength );

        std::vector<char> messageData( messageLength );
        auto*             message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>( messageData.data() );

        if ( SUCCEEDED( dxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, message, &messageLength ) ) )
            ss << message->pDescription << "\n";
    }

    dxgiInfoQueue->ClearStoredMessages( DXGI_DEBUG_ALL );
    return ss.str();
}

/// @brief Returns the Windows description of an HRESULT. Empty if Windows doesn't know it.
inline std::string getHResultMessage( HRESULT hr ) {
    char*       buffer = nullptr;
    const DWORD length = FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                         nullptr,
                                         (DWORD)hr,
                                         0,
                                         reinterpret_cast<char*>( &buffer ),
                                         0,
                                         nullptr );
    if ( length == 0 )
        return {};

    std::string message( buffer, length );
    LocalFree( buffer );

    // The description ends with a line break
    while ( !message.empty() && ( message.back() == '\n' || message.back() == '\r' ) )
        message.pop_back();
    return message;
}

/// @brief Describes a failed DirectX call: what failed, why and where.
inline std::string describeFailure( HRESULT hr, const char* call, const char* file, int line ) {
    std::string message = fmt::format( "DirectX12 call failed: {}\nError 0x{:08X}: {}\nAt {}:{}", call, (unsigned long)hr, getHResultMessage( hr ), file, line );

    const std::string dxgiMessage = getLastDXGIMessage();
    if ( !dxgiMessage.empty() )
        message += fmt::format( "\nDXGI Layer: {}", dxgiMessage );
    return message;
}

} // namespace Graphics::RHI::Debug

AXION_NAMESPACE_END

/// @brief Ends the application if a DirectX call fails, reporting it in every build configuration.
/// AXION_LOG_ERROR is what keeps the error in the log file of a Debug build.
#define DX_CHECK( x )                                                                                                 \
    do                                                                                                                \
    {                                                                                                                 \
        HRESULT hr__ = ( x );                                                                                         \
        if ( FAILED( hr__ ) )                                                                                         \
        {                                                                                                             \
            const std::string error__ = Axion::Graphics::RHI::Debug::describeFailure( hr__, #x, __FILE__, __LINE__ ); \
            AXION_LOG_ERROR( Axion::Logger::Module::RHI, "{}", error__ );                                             \
            AXION_LOG_FATAL_ERROR( Axion::Logger::Module::RHI, "{}", error__ );                                       \
            Axion::Logger::flush();                                                                                   \
            AXION_FATAL_ERROR_POPUP_WINDOW( "{}", error__ );                                                          \
            abort();                                                                                                  \
        }                                                                                                             \
    } while ( 0 )