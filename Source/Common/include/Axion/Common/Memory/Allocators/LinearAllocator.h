#pragma once
#include <Axion/Common/Helpers.h>
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <Axion/Common/Memory/VMemoryArena.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class LinearAllocator : public IAllocator, public LockPolicy
{
public:
    LinearAllocator( VMemoryArena* arena, uint arenaOffset, uint maxCapacity )
        : _arena( arena )
        , _ARENA_OFFSET( arenaOffset )
        , _MAX_CAPACITY( maxCapacity ) {
    }

    ~LinearAllocator() override {
        reset();
    }

    void* allocate( uint size, uint alignment = 16 ) override {

        this->lock();

        uint alignedOffset = Helpers::alignu( _currentOffset, alignment );
        if ( alignedOffset + size > _MAX_CAPACITY )
        {
            this->unlock();
            AXION_LOG_ERROR( Logger::Module::Common, "LinearAllocator OOM! Offset: {}, Size: {}, Capacity: {}", alignedOffset, size, _MAX_CAPACITY );
            return nullptr;
        }

        uint currentAbsoluteOffset = _ARENA_OFFSET + alignedOffset;

        if ( !_arena->commitRange( currentAbsoluteOffset, size ) )
        {
            this->unlock();
            return nullptr;
        }

        void* newAlloc = static_cast<uchar*>( _arena->getBasePtr() ) + ( currentAbsoluteOffset );

        _currentOffset = alignedOffset + size;

        this->unlock();
        return newAlloc;
    }

    void free( void* ptr ) override {
        AXION_UNUSED_PARAMETER( ptr );
        AXION_LOG_ASSERT( true, Logger::Module::Common, "Linear Allocator does not support free()" )
    }

    void reset() override {
        this->lock();

        if ( _currentOffset > 0 )
            _arena->decommitRange( _ARENA_OFFSET, _currentOffset );

        _currentOffset = 0;
        this->unlock();
    }

    uint getUsedSize() const override {
        this->lock();
        uint used = _currentOffset;
        this->unlock();
        return used;
    }

    uint getTotalSize() const override { return _MAX_CAPACITY; }

private:
    VMemoryArena* _arena;
    const uint    _ARENA_OFFSET;
    const uint    _MAX_CAPACITY;

    uint _currentOffset = 0;
};

using LockedLinearAllocator = LinearAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END