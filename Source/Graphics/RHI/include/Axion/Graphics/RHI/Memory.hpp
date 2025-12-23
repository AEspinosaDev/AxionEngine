#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Resource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

struct BufferView {
    ulong    gpuAddress = 0;
    uchar*   cpuAddress = nullptr;
    ulong    offset     = 0;
    IBuffer* buffer     = nullptr;

    bool isValid() const { return buffer != nullptr; }
};

class BufferLinearAllocator
{
public:
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

    AXION_FORCE_INLINE BufferView allocate( ulong size, ulong alignment = 256 ) {

        ulong alignedOffset = ( _currentOffset + ( alignment - 1 ) ) & ~( alignment - 1 );

        if ( alignedOffset + size > _capacity )
        {
            AXION_LOG_ERROR( Logger::Module::RHI,
                             "LinearAllocator with buffer [{}] Overflow! Request: {}, Available: {}",
                             _buffer->getDebugName(),
                             size,
                             _capacity - alignedOffset );
            return {};
        }

        BufferView alloc;
        alloc.buffer     = _buffer;
        alloc.offset     = alignedOffset;
        alloc.gpuAddress = _gpuBase + alignedOffset;

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

    std::string _debugName;
    ulong       _gpuBase = 0;
    uchar*      _cpuBase = nullptr;

    ulong _capacity      = 0;
    ulong _currentOffset = 0;
};

typedef BufferLinearAllocator LinearAllocator;

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
