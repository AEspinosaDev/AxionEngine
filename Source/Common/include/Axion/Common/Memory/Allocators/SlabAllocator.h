#pragma once
#include <Axion/Common/Memory/Allocators/PoolAllocator.h>
#include <array>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class SlabAllocator : public IAllocator, public LockPolicy
{
public:
    SlabAllocator( VMemoryArena* arena, uint arenaOffset, uint totalCapacity )
        : _arena( arena )
        , _arenaOffset( arenaOffset )
        , _maxCapacity( totalCapacity )
        , _capacityPerSlab( totalCapacity / NUM_SLABS ) {

        uint chunkSizes[NUM_SLABS] = { 16, 32, 64, 128, 256, 512 };

        for ( uint i = 0; i < NUM_SLABS; ++i )
            _pools[i].init( arena, arenaOffset + ( _capacityPerSlab * i ), _capacityPerSlab, chunkSizes[i] );
    }

    ~SlabAllocator() override{
        reset();
    };

    void* allocate( uint size, uint alignment = 16 ) override {
        this->lock();

        uint poolIndex = getPoolIndex( size );
        if ( poolIndex < NUM_SLABS )
        {
            void* ptr = _pools[poolIndex]->allocate( size, alignment );
            this->unlock();
            return ptr;
        }

        this->unlock();
        AXION_LOG_ERROR( Logger::Module::Common, "SlabAllocator request too large ({} bytes). Max supported is 512.", size );
        return nullptr;
    }

    void free( void* ptr ) override {
        if ( !ptr )
            return;

        this->lock();

        VMemoryAddress ptrAddr  = reinterpret_cast<VMemoryAddress>( ptr );
        VMemoryAddress baseAddr = reinterpret_cast<VMemoryAddress>( _arena->getBasePtr() ) + _arenaOffset;

        if ( ptrAddr >= baseAddr && ptrAddr < baseAddr + _maxCapacity )
        {
            uint offset    = static_cast<uint>( ptrAddr - baseAddr );
            uint poolIndex = offset / _capacityPerSlab;

            _pools[poolIndex]->free( ptr );
        } else
        {
            AXION_LOG_ERROR( Logger::Module::Common, "Pointer does not belong to this SlabAllocator" );
        }

        this->unlock();
    }

    void reset() override {
        this->lock();
        for ( auto* pool : _pools )
            pool->reset();
        this->unlock();
    }

    uint getUsedSize() const override {
        this->lock();
        uint totalUsed = 0;
        for ( const auto* pool : _pools )
            totalUsed += pool->getUsedSize();
        this->unlock();
        return totalUsed;
    }

    uint getTotalSize() const override { return _MAX_CAPACITY; }

private:
    static constexpr uint getPoolIndex( uint size ) {
        if ( size <= 16 )
            return 0;
        if ( size <= 32 )
            return 1;
        if ( size <= 64 )
            return 2;
        if ( size <= 128 )
            return 3;
        if ( size <= 256 )
            return 4;
        if ( size <= 512 )
            return 5;
        return 0xFFFF;
    }

    VMemoryArena* _arena;
    uint       _arenaOffset;
    uint       _maxCapacity;
    uint       _capacityPerSlab;

    static constexpr uint                              NUM_SLABS = 6;
    std::array<PoolAllocator<NoLockPolicy>, NUM_SLABS> _pools;
};

using LockedSlabAllocator = SlabAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END