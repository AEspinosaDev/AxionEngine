#pragma once
#include <Axion/Common/Helpers.h>
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <Axion/Common/Memory/VMemoryArena.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class FreeListAllocator : public IAllocator, public LockPolicy
{
private:
    // Placed at the start of every active allocation
    struct AllocationHeader {
        uint size;
        uint padding;
    };

    // Placed at the start of every free block
    struct FreeNode {
        uint      size;
        FreeNode* next;
    };

public:
    FreeListAllocator( VMemoryArena* arena, uint arenaOffset, uint maxCapacity )
        : _arena( arena )
        , _ARENA_OFFSET( arenaOffset )
        , _MAX_CAPACITY( maxCapacity )
        , _allocatedSize( 0 )
        , _freeList( nullptr ) {
    }

    ~FreeListAllocator() override {
        reset();
    }

    void* allocate( uint size, uint alignment = 16 ) override {
        this->lock();

        FreeNode* prevNode = nullptr;
        FreeNode* currNode = _freeList;

        while ( currNode != nullptr )
        {
            VMemoryAddress currAddress     = reinterpret_cast<VMemoryAddress>( currNode );
            uint          requiredPadding = calculatePaddingWithHeader( currAddress, alignment, sizeof( AllocationHeader ) );
            uint          requiredSpace   = size + requiredPadding;

            if ( currNode->size >= requiredSpace )
            {
                uint remainingSize = currNode->size - requiredSpace;

                if ( remainingSize > sizeof( FreeNode ) )
                {
                    // Split the block
                    FreeNode* newNode = reinterpret_cast<FreeNode*>( currAddress + requiredSpace );
                    newNode->size     = remainingSize;
                    newNode->next     = currNode->next;

                    if ( prevNode != nullptr )
                        prevNode->next = newNode;
                    else
                        _freeList = newNode;
                } else
                {
                    // Block is too small to split, absorb the whole thing
                    requiredSpace = currNode->size;
                    if ( prevNode != nullptr )
                        prevNode->next = currNode->next;
                    else
                        _freeList = currNode->next;
                }

                VMemoryAddress     alignedAddress = currAddress + requiredPadding;
                AllocationHeader* header         = reinterpret_cast<AllocationHeader*>( alignedAddress - sizeof( AllocationHeader ) );

                header->size    = requiredSpace;
                header->padding = requiredPadding;

                _activeAllocations++;
                this->unlock();
                return reinterpret_cast<void*>( alignedAddress );
            }

            prevNode = currNode;
            currNode = currNode->next;
        }

        // 2. Fallback: Allocate from the uncommitted bump pointer
        uint          currentAbsoluteOffset = _ARENA_OFFSET + _allocatedSize;
        VMemoryAddress bumpAddress           = reinterpret_cast<VMemoryAddress>( _arena->getBasePtr() ) + currentAbsoluteOffset;

        uint requiredPadding = calculatePaddingWithHeader( bumpAddress, alignment, sizeof( AllocationHeader ) );
        uint requiredSpace   = size + requiredPadding;

        if ( _allocatedSize + requiredSpace > _MAX_CAPACITY )
        {
            this->unlock();
            AXION_LOG_ERROR( Logger::Module::Common, "FreeListAllocator OOM! Offset: {}, Space Needed: {}, Capacity: {}", currentAbsoluteOffset, requiredSpace, _MAX_CAPACITY );
            return nullptr;
        }

        // Commit the required range
        if ( !_arena->commitRange( currentAbsoluteOffset, requiredSpace ) )
        {
            this->unlock();
            return nullptr;
        }

        VMemoryAddress     alignedAddress = bumpAddress + requiredPadding;
        AllocationHeader* header         = reinterpret_cast<AllocationHeader*>( alignedAddress - sizeof( AllocationHeader ) );

        header->size    = requiredSpace;
        header->padding = requiredPadding;

        _allocatedSize += requiredSpace;
        _activeAllocations++;

        this->unlock();
        return reinterpret_cast<void*>( alignedAddress );
    }

    void free( void* ptr ) override {
        if ( !ptr )
            return;

        this->lock();

        VMemoryAddress ptrAddress = reinterpret_cast<VMemoryAddress>( ptr );

        // Retrieve the hidden header
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>( ptrAddress - sizeof( AllocationHeader ) );

        // Find the absolute start of the block
        VMemoryAddress blockStart = ptrAddress - header->padding;
        uint          blockSize  = header->size;

        FreeNode* freeNode = reinterpret_cast<FreeNode*>( blockStart );
        freeNode->size     = blockSize;

        insertAndCoalesce( freeNode );

        _activeAllocations--;
        this->unlock();
    }

    void reset() override {
        this->lock();
        if ( _allocatedSize > 0 )
            _arena->decommitRange( _ARENA_OFFSET, _allocatedSize );
        _freeList          = nullptr;
        _allocatedSize     = 0;
        _activeAllocations = 0;
        this->unlock();
    }

    uint getUsedSize() const override {
        this->lock();
        uint used = _allocatedSize;
        this->unlock();
        return used;
    }

    uint getTotalSize() const override { return _MAX_CAPACITY; }

private:
    // Inserts a node back into the linked list maintaining address order, then merges neighbors
    void insertAndCoalesce( FreeNode* newNode ) {
        FreeNode* prevNode = nullptr;
        FreeNode* currNode = _freeList;

        // Find the right spot based on memory address
        while ( currNode != nullptr && currNode < newNode )
        {
            prevNode = currNode;
            currNode = currNode->next;
        }

        // Insert
        if ( prevNode != nullptr )
        {
            prevNode->next = newNode;
        } else
        {
            _freeList = newNode;
        }
        newNode->next = currNode;

        VMemoryAddress newAddress = reinterpret_cast<VMemoryAddress>( newNode );

        // Coalesce with next
        if ( newNode->next != nullptr )
        {
            VMemoryAddress nextAddress = reinterpret_cast<VMemoryAddress>( newNode->next );
            if ( newAddress + newNode->size == nextAddress )
            {
                newNode->size += newNode->next->size;
                newNode->next = newNode->next->next;
            }
        }

        // Coalesce with previous
        if ( prevNode != nullptr )
        {
            VMemoryAddress prevAddress = reinterpret_cast<VMemoryAddress>( prevNode );
            if ( prevAddress + prevNode->size == newAddress )
            {
                prevNode->size += newNode->size;
                prevNode->next = newNode->next;
            }
        }
    }

    uint calculatePaddingWithHeader( VMemoryAddress ptr, uint alignment, uint headerSize ) const {
        uint padding = alignment - ( ptr % alignment );
        if ( padding == alignment )
            padding = 0;

        uint neededSpace = headerSize;
        if ( padding < neededSpace )
        {
            neededSpace -= padding;
            if ( neededSpace % alignment > 0 )
                padding += alignment * ( 1 + ( neededSpace / alignment ) );
            else
                padding += alignment * ( neededSpace / alignment );
        }
        return padding;
    }

    VMemoryArena* _arena;
    const uint _ARENA_OFFSET;
    const uint _MAX_CAPACITY;

    FreeNode* _freeList;
    uint      _allocatedSize     = 0;
    uint      _activeAllocations = 0; // Tracks actual blocks in use
};

using LockedFreeListAllocator = FreeListAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END