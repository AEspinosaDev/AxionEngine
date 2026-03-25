#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Common/Memory/IAllocator.h"
#include "Axion/Common/Memory/MemoryManager.h"

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
    FreeListAllocator( uint capacity ) {
        _pool = VMemManager::virtualReserve( capacity );
        if ( !VMemManager::virtualCommit( _pool ) )
            throw AxionException( "Failed to commit virtual memory for FreeListAllocator" );

        _usedSize = 0;

        _head       = static_cast<FreeNode*>( _pool.ptr );
        _head->size = _pool.size;
        _head->next = nullptr;
    }

    ~FreeListAllocator() override {
        VMemManager::virtualDecommit( _pool );
        VMemManager::virtualRelease( _pool );
    }

    void* allocate( uint size, uint alignment = 16 ) override {
        this->lock();

        if ( !_pool.isValid() || size == 0 )
        {
            this->unlock();
            return nullptr;
        }

        FreeNode* prevNode = nullptr;
        FreeNode* currNode = _head;

        while ( currNode != nullptr )
        {
            MemoryAddress currAddress = reinterpret_cast<MemoryAddress>( currNode );

            uint requiredPadding = calculatePaddingWithHeader(
                currAddress, alignment, sizeof( AllocationHeader ) );

            uint requiredSpace = size + requiredPadding;

            if ( currNode->size >= requiredSpace )
            {
                uint remainingSize = currNode->size - requiredSpace;

                if ( remainingSize > sizeof( FreeNode ) )
                {
                    FreeNode* newNode = reinterpret_cast<FreeNode*>( currAddress + requiredSpace );
                    newNode->size     = remainingSize;
                    newNode->next     = currNode->next;

                    if ( prevNode != nullptr )
                        prevNode->next = newNode;
                    else
                        _head = newNode;
                } else
                {
                    // Block is too small to split, just eat the whole thing
                    requiredSpace = currNode->size; // Adjust padding/size internally
                    if ( prevNode != nullptr )
                        prevNode->next = currNode->next;
                    else
                        _head = currNode->next;
                }

                // Setup the allocation header and return the aligned pointer
                MemoryAddress     alignedAddress = currAddress + requiredPadding;
                AllocationHeader* header         = reinterpret_cast<AllocationHeader*>( alignedAddress - sizeof( AllocationHeader ) );

                header->size    = requiredSpace;
                header->padding = requiredPadding;

                _usedSize += requiredSpace;

                this->unlock();
                return reinterpret_cast<void*>( alignedAddress );
            }

            prevNode = currNode;
            currNode = currNode->next;
        }

        AXION_LOG_ERROR( Logger::Module::Common, "FreeListAllocator OOM! Request: {}", size );
        this->unlock();
        return nullptr;
    }

    void free( void* ptr ) override {
        if ( !ptr )
            return;

        this->lock();

        MemoryAddress ptrAddress = reinterpret_cast<MemoryAddress>( ptr );

        // Retrieve the hidden header
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>( ptrAddress - sizeof( AllocationHeader ) );

        // Find the absolute start of the block
        MemoryAddress blockStart = ptrAddress - header->padding;
        uint          blockSize  = header->size;

        _usedSize -= blockSize;

        FreeNode* freeNode = reinterpret_cast<FreeNode*>( blockStart );
        freeNode->size     = blockSize;

        insertAndCoalesce( freeNode );

        this->unlock();
    }

    void reset() override {
        this->lock();
        _head       = static_cast<FreeNode*>( _pool.ptr );
        _head->size = _pool.size;
        _head->next = nullptr;
        _usedSize   = 0;
        this->unlock();
    }

    uint getUsedSize() const override {
        this->lock();
        uint used = _usedSize;
        this->unlock();
        return used;
    }

    uint getTotalSize() const override { return _pool.size; }

private:
    // Inserts a node back into the linked list maintaining address order, then merges neighbors
    void insertAndCoalesce( FreeNode* newNode ) {
        FreeNode* prevNode = nullptr;
        FreeNode* currNode = _head;

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
            _head = newNode;
        }
        newNode->next = currNode;

        MemoryAddress newAddress = reinterpret_cast<MemoryAddress>( newNode );

        // Coalesce with next
        if ( newNode->next != nullptr )
        {
            MemoryAddress nextAddress = reinterpret_cast<MemoryAddress>( newNode->next );
            if ( newAddress + newNode->size == nextAddress )
            {
                newNode->size += newNode->next->size;
                newNode->next = newNode->next->next;
            }
        }

        // Coalesce with previous
        if ( prevNode != nullptr )
        {
            MemoryAddress prevAddress = reinterpret_cast<MemoryAddress>( prevNode );
            if ( prevAddress + prevNode->size == newAddress )
            {
                prevNode->size += newNode->size;
                prevNode->next = newNode->next;
            }
        }
    }

    uint calculatePaddingWithHeader( MemoryAddress ptr, uint alignment, uint headerSize ) const {
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

    VMemView  _pool;
    FreeNode* _head;
    uint      _usedSize;
};

using LockedFreeListAllocator = FreeListAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END