#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Resource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

struct BufferView {

    IBuffer* buffer = nullptr;
    ulong    size   = 0;
    ulong    stride = 0;
    ulong    count  = 0;
    ulong    offset = 0;

    ulong  gpuAddress = 0;
    uchar* cpuAddress = nullptr;

    bool isValid() const { return buffer != nullptr; }
};

#pragma region LinearAllocator

class BufferLinearAllocator
{
public:
    BufferLinearAllocator() {};
    BufferLinearAllocator( IBuffer* buffer )
        : _buffer( buffer ) {

        AXION_LOG_ASSERT( buffer, Logger::Module::RHI, "LinearAllocator initialized with null buffer" );

        _capacity = buffer->getDescription().size;
        _gpuBase  = buffer->getDeviceAddress();

        if ( buffer->getDescription().memoryType != MemoryUsage::GPUOnly )
        {
            _cpuBase = static_cast<uchar*>( buffer->getData() );
        }
    }

    template <typename T>
    AXION_FORCE_INLINE BufferView allocate( ulong count ) {
        return allocate( count * sizeof( T ), sizeof( T ) );
    }

    AXION_FORCE_INLINE BufferView allocate( ulong size, ulong alignment = 256 ) {

        ulong alignedOffset = Helpers::safeAlign( _currentOffset, alignment );

        if ( alignedOffset + size > _capacity )
        {
            AXION_LOG_ERROR( Logger::Module::RHI,
                             "LinearAllocator with buffer [{}] Overflow! Request: {}, Available: {}",
                             _buffer->getDebugName(),
                             size,
                             _capacity - _currentOffset );
            return {};
        }

        BufferView alloc;
        alloc.buffer     = _buffer;
        alloc.offset     = alignedOffset;
        alloc.size       = size;
        alloc.gpuAddress = _gpuBase + alignedOffset;

        if ( alignment > 0 )
        {
            alloc.stride = alignment;
            alloc.count  = size / alignment;
        } else
        {
            alloc.stride = size;
            alloc.count  = 1;
        }

        if ( _cpuBase )
            alloc.cpuAddress = _cpuBase + alignedOffset;
        else
            alloc.cpuAddress = nullptr;

        _currentOffset = alignedOffset + size;

        return alloc;
    }

    void reset() {
        _currentOffset = 0;
    }

    AXION_FORCE_INLINE ulong getUsedSize() const { return _currentOffset; }
    AXION_FORCE_INLINE ulong getTotalSize() const { return _capacity; }

private:
    IBuffer* _buffer;

    ulong       _gpuBase = 0;
    uchar*      _cpuBase = nullptr;

    ulong _capacity      = 0;
    ulong _currentOffset = 0;
};

typedef BufferLinearAllocator LinearAllocator;

#pragma endregion

#pragma region FreeListAllocator


class BufferFreeListAllocator
{
    struct FreeBlock
    {
        ulong offset;
        ulong size;
    };

public:
    BufferFreeListAllocator() {};
    
    BufferFreeListAllocator( IBuffer* buffer )
        : _buffer( buffer ) {

        AXION_LOG_ASSERT( buffer, Logger::Module::RHI, "FreeListAllocator initialized with null buffer" );

        _capacity = buffer->getDescription().size;
        _gpuBase  = buffer->getDeviceAddress();

        if ( buffer->getDescription().memoryType != MemoryUsage::GPUOnly )
        {
            _cpuBase = static_cast<uchar*>( buffer->getData() );
        }

        _freeBlocks.push_back( { 0, _capacity } );
    }

    template <typename T>
    AXION_FORCE_INLINE BufferView allocate( ulong count ) {
        return allocate( count * sizeof( T ), sizeof( T ) );
    }

    BufferView allocate( ulong size, ulong alignment = 256 ) {
        
        for ( auto it = _freeBlocks.begin(); it != _freeBlocks.end(); ++it )
        {
            ulong alignedOffset = Helpers::safeAlign( it->offset, alignment );
            ulong padding       = alignedOffset - it->offset;
            ulong requiredSize  = size + padding;

            if ( it->size >= requiredSize )
            {

                BufferView alloc;
                alloc.buffer     = _buffer;
                alloc.offset     = alignedOffset;
                alloc.size       = size;
                alloc.gpuAddress = _gpuBase + alignedOffset;
                
                if ( alignment > 0 ) {
                    alloc.stride = alignment;
                    alloc.count  = size / alignment;
                } else {
                    alloc.stride = size;
                    alloc.count  = 1;
                }

                if ( _cpuBase ) alloc.cpuAddress = _cpuBase + alignedOffset;

                // Update Free Slot
                ulong totalConsumed = requiredSize;
                ulong remainingSize = it->size - totalConsumed;

                if ( remainingSize > 0 )
                {
                    it->offset += totalConsumed;
                    it->size   = remainingSize;
                }
                else
                {
                    _freeBlocks.erase( it );
                }

                _usedSize += size; // Solo contamos lo útil, no el padding
                return alloc;
            }
        }

       
        AXION_LOG_ERROR( Logger::Module::RHI,
             "FreeListAllocator [{}] OOM! Request: {}, Max Free Block available: {}",
             _buffer->getDebugName(), size, getMaxFreeBlockSize() );

        return {};
    }

    void free( const BufferView& view ) {
        if ( !view.isValid() || view.buffer != _buffer )
        {
            AXION_LOG_WARN( Logger::Module::RHI, "Trying to free invalid view or view from another buffer" );
            return;
        }

        FreeBlock newBlock = { view.offset, view.size };
        
        // Fusion(Coalescing)
        insertAndCoalesce( newBlock );

        _usedSize -= view.size;
    }

    void reset() {
        _freeBlocks.clear();
        _freeBlocks.push_back( { 0, _capacity } );
        _usedSize = 0;
    }
    
    ulong getUsedSize() const { return _usedSize; }
    ulong getTotalSize() const { return _capacity; }

private:
    
    void insertAndCoalesce( FreeBlock block ) {
        
        auto it = std::upper_bound( _freeBlocks.begin(), _freeBlocks.end(), block.offset,
            []( ulong val, const FreeBlock& b ) { return val < b.offset; } 
        );

        it = _freeBlocks.insert( it, block );

        
        auto next = it;
        ++next;
        
        //Eats next one if possible
        if ( next != _freeBlocks.end() && (it->offset + it->size == next->offset) ) {
            it->size += next->size; 
            _freeBlocks.erase( next ); 
        }

        //Prev eats current
        if ( it != _freeBlocks.begin() ) {
            auto prev = it;
            --prev;
            if ( prev->offset + prev->size == it->offset ) {
                prev->size += it->size; 
                _freeBlocks.erase( it ); 
            }
        }
    }

    ulong getMaxFreeBlockSize() const {
        ulong maxS = 0;
        for(const auto& b : _freeBlocks) if(b.size > maxS) maxS = b.size;
        return maxS;
    }

    IBuffer* _buffer = nullptr;
    ulong       _gpuBase = 0;
    uchar* _cpuBase = nullptr;

    ulong _capacity = 0;
    ulong _usedSize = 0;

    std::vector<FreeBlock> _freeBlocks; 
};
typedef BufferFreeListAllocator FreeListAllocator;

#pragma endregion

DEFINE_COM_PTR_FOR_TYPE( ITransientAllocator, TransientAllocator )
/**
 * @brief Manages transient memory for a single frame (Scratch & Upload heaps).
 * Automatically resets at the start of the frame. Useful for data streaming
 */
class ITransientAllocator : public IResource
{
public:
    struct Description {
        ulong       scratchSize = 64 * 1024 * 1024;
        ulong       uploadSize  = 64 * 1024 * 1024;
        std::string debugName;
    };

    virtual ~ITransientAllocator() = default;

    virtual BufferView allocateScratch( ulong size, ulong alignment = 256 ) = 0;
    virtual BufferView allocateUpload( ulong size, ulong alignment = 256 )  = 0;

    virtual void reset() = 0;

    virtual const Description& getDescription() const = 0;
};

typedef ITransientAllocator::Description TransientAllocatorDesc;
} // namespace Graphics::RHI

AXION_NAMESPACE_END

// #pragma once
// #include "Axion/Memory/MemorySubAlloc.h"
// #include "Axion/RHI/Buffer.h" // Assuming this is your DX12 buffer wrapper

// AXION_NAMESPACE_BEGIN
// namespace RHI {

// template <
//     typename LockPolicy = Memory::NoLock, 
//     typename VisibilityPolicy = Memory::DeviceLocal
// >
// class LinearBufferAllocator : public Memory::ISubAllocator<Buffer>, public LockPolicy {
// public:
//     LinearBufferAllocator(Buffer* targetBuffer) 
//         : _target(targetBuffer) {
//         _capacity = _target ? _target->getSize() : 0;
//     }

//     Memory::SubAllocation<Buffer> allocate(size_t size, size_t alignment = 256) override {
//         this->lock();

//         if (!_target) {
//             this->unlock();
//             return {};
//         }

//         // Align the current offset
//         size_t remainder = _currentOffset % alignment;
//         size_t padding = (remainder == 0) ? 0 : (alignment - remainder);
//         size_t alignedOffset = _currentOffset + padding;

//         if (alignedOffset + size > _capacity) {
//             this->unlock();
//             return {}; 
//         }

//         // Optional: Assert if user is trying to map memory that is DeviceLocal
//         if constexpr (!VisibilityPolicy::isCPUReadable) {
//             // Add custom debug logic here if needed
//         }

//         Memory::SubAllocation<Buffer> alloc;
//         alloc.container = _target;
//         alloc.offset = alignedOffset;
//         alloc.size = size;

//         _currentOffset = alignedOffset + size;

//         this->unlock();
//         return alloc;
//     }

//     void free(Memory::SubAllocation<Buffer>& allocation) override {
//         // Linear allocators cannot free individual blocks
//         (void)allocation;
//     }

//     void reset() override {
//         this->lock();
//         _currentOffset = 0;
//         this->unlock();
//     }

//     size_t getCapacity() const override { return _capacity; }
//     size_t getUsed() const override { return _currentOffset; }

// private:
//     Buffer* _target = nullptr;
//     size_t _capacity = 0;
//     size_t _currentOffset = 0;
// };

// } // namespace RHI
// AXION_NAMESPACE_END