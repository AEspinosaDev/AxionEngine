#pragma once
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <Axion/Common/Memory/VMemoryArena.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class PoolAllocator : public IAllocator, public LockPolicy
{
private:
    struct FreeNode {
        FreeNode* next;
    };

public:
    PoolAllocator() = default;
    PoolAllocator( VMemoryArena* arena, u32 arenaOffset, u32 maxCapacity, u32 chunkSize ) {
        initialize( arena, arenaOffset, maxCapacity, chunkSize );
    }
    // Two-step initialization
    void initialize( VMemoryArena* arena, u32 arenaOffset, u32 maxCapacity, u32 chunkSize ) {
        _arena         = arena;
        _arenaOffset   = arenaOffset;
        _maxCapacity   = maxCapacity;
        _allocatedSize = 0;
        _freeList      = nullptr;

        u32 alignmentOperator = AXION_MEMORY_MINIMUM_ALIGNMENT - 1;
        _chunkSize             = std::max( chunkSize, static_cast<u32>( sizeof( FreeNode ) ) );
        _chunkSize             = ( _chunkSize + alignmentOperator ) & ~alignmentOperator;
    }

    ~PoolAllocator() override {
        reset();
    }

    void* allocate( u32 size, u32 alignment ) override {
        AXION_UNUSED_PARAMETER( alignment );
        AXION_LOG_ASSERT( size <= _chunkSize, Logger::Module::Common, "Requested size {} exceeds pool chunk size {}", size, _chunkSize );

        this->lock();

        if ( _freeList != nullptr )
        {
            FreeNode* node = _freeList;
            _freeList      = _freeList->next;
            this->unlock();
            return node;
        }

        if ( _allocatedSize + _chunkSize > _maxCapacity )
        {
            this->unlock();
            AXION_LOG_ERROR( Logger::Module::Common, "PoolAllocator OOM! Offset: {}, Size: {}, Capacity: {}", _arenaOffset + _allocatedSize, _chunkSize, _maxCapacity );
            return nullptr;
        }

        u32 currentAbsoluteOffset = _arenaOffset + _allocatedSize;

        if ( !_arena->commitRange( currentAbsoluteOffset, _chunkSize ) )
        {
            this->unlock();
            return nullptr;
        }

        void* newBlock = static_cast<byte*>( _arena->getBasePtr() ) + currentAbsoluteOffset;
        _allocatedSize += _chunkSize;
        _activeAllocations++;

        this->unlock();
        return newBlock;
    }

    void free( void* ptr ) override {
        if ( !ptr )
            return;

        this->lock();
        // Push the block back to the front of the free list
        FreeNode* node = static_cast<FreeNode*>( ptr );
        node->next     = _freeList;
        _freeList      = node;
        _activeAllocations--;
        this->unlock();
    }

    void reset() override {
        this->lock();

        if ( _allocatedSize > 0 )
            _arena->decommitRange( _arenaOffset, _allocatedSize );

        _freeList      = nullptr;
        _allocatedSize = 0;
        this->unlock();
    }

    u32 getUsedSize() const override { return _allocatedSize * _activeAllocations; }
    u32 getTotalSize() const override { return _maxCapacity; }

private:
    VMemoryArena* _arena;
    const u32    _arenaOffset;
    const u32    _maxCapacity;

    u32 _allocatedSize;
    u32 _chunkSize;
    u32 _activeAllocations = 0; // Tracks actual objects in use

    FreeNode* _freeList;
};

using LockedPoolAllocator = PoolAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END