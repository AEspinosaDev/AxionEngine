#pragma once
#include <Axion/Common/Helpers.h>
#include <Axion/Common/Memory/Allocators/SubAllocators/ISubAllocator.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename TContainer,
          typename VisibilityPolicy = VisibilityHostOnly,
          typename LockPolicy       = NoLockPolicy>

class LinearSubAllocator : public ISubAllocator<TContainer, VisibilityPolicy>, public LockPolicy
{
public:
    LinearSubAllocator() = default;
    LinearSubAllocator( TContainer* container ) { initialize( container ); }

    void initialize( TContainer* container ) {
        _container   = container;
        _maxCapacity = container->getCapacity();
        if constexpr ( VisibilityPolicy::isGPUVisible )
            _gpuBase = container->getDeviceAddress();

        if constexpr ( VisibilityPolicy::isCPUReadable )
            _cpuBase = static_cast<byte*>( container->getHostAddress() );
    }

    ~LinearSubAllocator() {
        reset();
    }
    template <typename TItem>
    SubAllocation<TContainer>         allocate( u64 count ) { return allocate( count * sizeof( TItem ), sizeof( TItem ) ); }
    virtual SubAllocation<TContainer> allocate( u64 size, u64 alignment = 256 ) override {
        this->lock();

        u64 alignedOffset = Helpers::safeAlign( _currentOffset, alignment );

        if ( alignedOffset + size > _maxCapacity )
        {

            this->unlock();
            AXION_LOG_ERROR( Logger::Module::RHI,
                             "Linear SubAllocator with Container [{}] Overflow! Request: {}, Available: {}",
                             _container->getDebugName(),
                             size,
                             _maxCapacity - _currentOffset );
            return {};
        }

        SubAllocation<TContainer> alloc;
        alloc.container = _container;

        alloc.offset = alignedOffset;
        alloc.size   = size;

        if constexpr ( VisibilityPolicy::isGPUVisible )
            alloc.gpuAddress = _gpuBase + alignedOffset;

        if constexpr ( VisibilityPolicy::isCPUReadable )
            alloc.cpuAddress = _cpuBase + alignedOffset;

        if ( alignment > 0 )
        {
            alloc.stride = alignment;
            alloc.count  = size / alignment;
        } else
        {
            alloc.stride = size;
            alloc.count  = 1;
        }

        _currentOffset = alignedOffset + size;

        this->unlock();

        return alloc;
    };
    virtual void free( SubAllocation<TContainer>& allocation ) override {
        AXION_UNUSED_PARAMETER( allocation );
        AXION_LOG_ASSERT( true, Logger::Module::Common, "LinearSubAllocator does not support free()" );
    };
    virtual void reset() override {
        this->lock();
        _currentOffset = 0;
        this->unlock();
    };

    virtual u64 getCapacity() const override {
        return _maxCapacity;
    }

    virtual u64 getUsed() const override { // Added const and override
        return _currentOffset;
    }

protected:
    TContainer* _container = nullptr;

    u64 _currentOffset = 0;
    u64 _maxCapacity   = 0;

    u64   _gpuBase = 0;
    byte* _cpuBase = nullptr;
};

} // namespace Memory
AXION_NAMESPACE_END