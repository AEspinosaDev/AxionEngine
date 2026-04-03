#pragma once
#include <Axion/Common/Helpers.h>
#include <Axion/Common/Memory/Allocators/SubAllocators/ISubAllocator.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename TContainer,
          typename VisibilityPolicy = VisibilityHostOnly,
          typename LockPolicy       = NoLockPolicy>

class FreeListSubAllocator : public ISubAllocator<TContainer, VisibilityPolicy>, public LockPolicy
{
public:
    FreeListSubAllocator() = default;
    FreeListSubAllocator( TContainer* container ) { initialize( container ); }

    void initialize( TContainer* container ) {
        _container   = container;
        _maxCapacity = container->getCapacity();
        if constexpr ( VisibilityPolicy::isGPUVisible )
            _gpuBase = container->getDeviceAddress();

        if constexpr ( VisibilityPolicy::isCPUReadable )
            _cpuBase = static_cast<byte*>( container->getHostAddress() );

        _freeBlocks.clear();
        _freeBlocks.push_back( { 0, _maxCapacity } );
    }
    ~FreeListSubAllocator() override {
        reset();
    }

    template <typename TItem>
    SubAllocation<TContainer>         allocate( u64 count ) { return allocate( count * sizeof( TItem ), sizeof( TItem ) ); }
    virtual SubAllocation<TContainer> allocate( u64 size, u64 alignment = 256 ) override {
        this->lock();

        for ( auto it = _freeBlocks.begin(); it != _freeBlocks.end(); ++it )
        {
            u64 alignedOffset = Helpers::safeAlign( it->offset, alignment );
            u64 padding       = alignedOffset - it->offset;
            u64 requiredSize  = size + padding;

            if ( it->size >= requiredSize )
            {

                SubAllocation<TContainer> alloc;
                alloc.container = _container;
                alloc.offset    = alignedOffset;
                alloc.size      = size;

                if ( alignment > 0 )
                {
                    alloc.stride = alignment;
                    alloc.count  = size / alignment;
                } else
                {
                    alloc.stride = size;
                    alloc.count  = 1;
                }

                if constexpr ( VisibilityPolicy::isGPUVisible )
                    alloc.gpuAddress = _gpuBase + alignedOffset;

                if constexpr ( VisibilityPolicy::isCPUReadable )
                    alloc.cpuAddress = _cpuBase + alignedOffset;

                // Update Free Slot
                u64 totalConsumed = requiredSize;
                u64 remainingSize = it->size - totalConsumed;

                if ( remainingSize > 0 )
                {
                    it->offset += totalConsumed;
                    it->size = remainingSize;
                } else
                {
                    _freeBlocks.erase( it );
                }

                _usedSize += size; // Solo contamos lo útil, no el padding
                this->unlock();
                return alloc;
            }
        }
        this->unlock();
        AXION_LOG_ERROR( Logger::Module::RHI,
                         "FreeList SubAllocator with Container [{}] Overflow! Request: {}, Available: {}",
                         _container->getDebugName(),
                         size,
                         getMaxFreeBlockSize() );
        return {};
    };
    virtual void free( SubAllocation<TContainer>& allocation ) override {
        if ( !allocation.isValid() || allocation.container != _container )
        {
            AXION_LOG_WARN( Logger::Module::RHI, "Trying to free invalid allocation or allocation from another buffer" );
            return;
        }
        this->lock();
        FreeBlock newBlock = { allocation.offset - allocation.padding, allocation.size + allocation.padding };

        // Fusion(Coalescing)
        insertAndCoalesce( newBlock );

        _usedSize -= allocation.size;

        this->unlock();
        allocation = {};
    };
    virtual void reset() override {
        this->lock();
        _freeBlocks.clear();
        _freeBlocks.push_back( { 0, _maxCapacity } );
        _usedSize = 0;
        this->unlock();
    };

    virtual u64 getCapacity() const override {
        return _maxCapacity;
    }

    virtual u64 getUsed() const override {
        return _usedSize;
    }

protected:
    struct FreeBlock {
        u64 offset;
        u64 size;
    };

    void insertAndCoalesce( FreeBlock block ) {

        auto it = std::upper_bound( _freeBlocks.begin(), _freeBlocks.end(), block.offset, []( u64 val, const FreeBlock& b ) { return val < b.offset; } );

        it = _freeBlocks.insert( it, block );

        auto next = it;
        ++next;

        // Eats next one if possible
        if ( next != _freeBlocks.end() && ( it->offset + it->size == next->offset ) )
        {
            it->size += next->size;
            _freeBlocks.erase( next );
        }

        // Prev eats current
        if ( it != _freeBlocks.begin() )
        {
            auto prev = it;
            --prev;
            if ( prev->offset + prev->size == it->offset )
            {
                prev->size += it->size;
                _freeBlocks.erase( it );
            }
        }
    }

    u64 getMaxFreeBlockSize() const {
        u64 maxS = 0;
        for ( const auto& b : _freeBlocks )
            if ( b.size > maxS )
                maxS = b.size;
        return maxS;
    }

    TContainer* _container = nullptr;

    u64 _usedSize    = 0;
    u64 _maxCapacity = 0;

    u64   _gpuBase = 0;
    byte* _cpuBase = nullptr;

    STLW::Vector<FreeBlock> _freeBlocks;
};

} // namespace Memory
AXION_NAMESPACE_END