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

} // namespace Graphics::RHI::Debug

AXION_NAMESPACE_END

#define DX_CHECK( x )                                                      \
    do                                                                     \
    {                                                                      \
        HRESULT hr__ = ( x );                                              \
        if ( FAILED( hr__ ) )                                              \
        {                                                                  \
            std::string dxgiMsg = Axion::Graphics::RHI::Debug::getLastDXGIMessage(); \
            AXION_LOG_ERROR( Axion::Logger::Module::RHI,                   \
                             "DirectX12 error: 0x{:X} DXGI Layer: {}",     \
                             hr__,                                         \
                             dxgiMsg );                                    \
            Axion::Logger::flush();                                        \
            abort();                                                       \
        }                                                                  \
    } while ( 0 )