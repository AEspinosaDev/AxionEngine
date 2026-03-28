#pragma once
#include <Axion/Common/Memory/Common.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

class VMemoryArena
{
public:
    VMemoryArena() = default;

    VMemoryArena( uint capacity );

    ~VMemoryArena();

    AXION_DISABLE_COPY( VMemoryArena )

    VMemoryArena( VMemoryArena&& other ) noexcept;

    VMemoryArena& operator=( VMemoryArena&& other ) noexcept;

    bool commitRange( uint offset, uint size );
    void decommitRange( uint offset, uint size );

    inline void* getBasePtr() const { return _reservation.ptr; }
    inline uint  getCapacity() const { return _reservation.size; }

private:
    VMemoryView _reservation;
    uint        _pageSize = 0;
};

} // namespace Memory
AXION_NAMESPACE_END