#pragma once
#include <Axion/Common/Memory/Allocators/PoolAllocator.h>
#include <array>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class SlabAllocator : public IAllocator, public LockPolicy
{
public:
    SlabAllocator( VMemoryArena* arena, u32 arenaOffset, u32 totalCapacity )
        : _arena( arena )
        , _arenaOffset( arenaOffset )
        , _maxCapacity( totalCapacity )
        , _capacityPerSlab( totalCapacity / NUM_SLABS ) {

        u32 chunkSizes[NUM_SLABS] = { 16, 32, 64, 128, 256, 512 };

        for ( u32 i = 0; i < NUM_SLABS; ++i )
            _pools[i].init( arena, arenaOffset + ( _capacityPerSlab * i ), _capacityPerSlab, chunkSizes[i] );
    }

    ~SlabAllocator() override{
        reset();
    };

    void* allocate( u32 size, u32 alignment = 16 ) override {
        this->lock();

        u32 poolIndex = getPoolIndex( size );
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
            u32 offset    = static_cast<u32>( ptrAddr - baseAddr );
            u32 poolIndex = offset / _capacityPerSlab;

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

    u32 getUsedSize() const override {
        this->lock();
        u32 totalUsed = 0;
        for ( const auto* pool : _pools )
            totalUsed += pool->getUsedSize();
        this->unlock();
        return totalUsed;
    }

    u32 getTotalSize() const override { return _MAX_CAPACITY; }

private:
    static constexpr u32 getPoolIndex( u32 size ) {
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
    u32       _arenaOffset;
    u32       _maxCapacity;
    u32       _capacityPerSlab;

    static constexpr u32                              NUM_SLABS = 6;
    std::array<PoolAllocator<NoLockPolicy>, NUM_SLABS> _pools;
};

using LockedSlabAllocator = SlabAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END