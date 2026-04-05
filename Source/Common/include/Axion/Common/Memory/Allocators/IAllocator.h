#pragma once
#include <Axion/Common/Memory/Common.h>

AXION_NAMESPACE_BEGIN

namespace Memory {

class IAllocator
{
public:
    virtual ~IAllocator() = default;

    virtual void* allocate( u64 size, u64 alignment = 16 ) = 0;
    virtual void  free( void* ptr )                            = 0;
    virtual void  reset()                                      = 0;

    virtual u64 getUsedSize() const  = 0;
    virtual u64 getTotalSize() const = 0;
};

} // namespace Memory
AXION_NAMESPACE_END