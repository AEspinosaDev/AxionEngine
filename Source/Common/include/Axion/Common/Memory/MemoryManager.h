#pragma once
#include "Axion/Common/Defines.h"

AXION_NAMESPACE_BEGIN
namespace Memory {

// Represents a raw block of memory from the OS
struct VMemView {
    void* ptr  = nullptr;
    uint  size = 0;
    bool  isValid() const { return ptr != nullptr && size > 0; }
};

//
class VMemManager
{
public:
    static uint getPageSize();
    static uint alignToPageSize( uint size );

    static VMemView virtualReserve( uint size );
    static bool     virtualCommit( VMemView view );
    static void     virtualDecommit( VMemView view );
    static void     virtualRelease( VMemView view );
};

} // namespace Memory
AXION_NAMESPACE_END

#if defined( _WIN32 )
#include "Backend/MemoryManager_Win32.inl"
#elif defined( __linux__ )
#include "Backend/MemoryManager_Linux.inl"
#else
#error "Platform not supported for MemoryManager!"
#endif