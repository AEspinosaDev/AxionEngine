#pragma once
#include "Defines.h"

AXION_NAMESPACE_BEGIN

namespace Memory {

// Lock Policies
// ----------------------------
struct NoLockPolicy {
    void lock() {}
    void unlock() {}
};

struct MutexLockPolicy {
    mutable std::mutex _mtx;
    void               lock() { _mtx.lock(); }
    void               unlock() { _mtx.unlock(); }
};

class IAllocator
{
public:
    virtual ~IAllocator() = default;

    virtual void* allocate( ulong size, ulong alignment = 16 ) = 0;
    virtual void  free( void* ptr )                            = 0;
    virtual void  reset()                                      = 0;

    virtual ulong getUsedSize() const  = 0;
    virtual ulong getTotalSize() const = 0;
};

} // namespace Memory
AXION_NAMESPACE_END