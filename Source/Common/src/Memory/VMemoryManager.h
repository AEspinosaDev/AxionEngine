#pragma once
#include <Axion/Common/Memory/Common.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

//
class VMemoryManager
{
public:
    static uint getPageSize();
    static uint alignToPageSize( uint size );

    static VMemoryView virtualReserve( uint size );
    static bool        virtualCommit( VMemoryView view );
    static void        virtualDecommit( VMemoryView view );
    static void        virtualRelease( VMemoryView view );
};

} // namespace Memory
AXION_NAMESPACE_END

#if defined( _WIN32 )
#include "Backend/VMemoryManager_Win32.inl"
#elif defined( __linux__ )
#include "Backend/MemoryManager_Linux.inl"
#else
#error "Platform not supported for MemoryManager!"
#endif