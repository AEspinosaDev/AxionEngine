#pragma once
#include <Axion/Common/Memory/Common.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

class VMemoryArena
{
public:
    VMemoryArena() = default;

    VMemoryArena( u32 capacity );

    ~VMemoryArena();

    AXION_DISABLE_COPY( VMemoryArena )

    VMemoryArena( VMemoryArena&& other ) noexcept;

    VMemoryArena& operator=( VMemoryArena&& other ) noexcept;

    bool commitRange( u32 offset, u32 size );
    void decommitRange( u32 offset, u32 size );

    inline void* getBasePtr() const { return _reservation.ptr; }
    inline u32  getCapacity() const { return _reservation.size; }

private:
    VMemoryView _reservation;
    u32        _pageSize = 0;
};

} // namespace Memory
AXION_NAMESPACE_END