#pragma once
#include <Axion/Common/Logging.h>
#include <Axion/Common/Memory/Allocator.h>
#include <Axion/Common/Memory/MemoryManager.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class LinearAllocator : public IAllocator, public LockPolicy
{
public:
    LinearAllocator( uint capacity ) {
        _backing = VMemManager::virtualReserve( capacity );
        if ( !VMemManager::virtualCommit( _backing ) )
        {
            throw AxionException();
        };

        _currentOffset = 0;
    }

    ~LinearAllocator() override {
        VMemManager::virtualDecommit( _backing );
        VMemManager::virtualRelease( _backing );
    }

    void* allocate( uint size, uint alignment = 16 ) override {
        this->lock();

        if ( !_backing.isValid() )
        {
            this->unlock();
            return nullptr;
        }

        void* currentPtr = static_cast<char*>( _backing.ptr ) + _currentOffset;
        uint  space      = _backing.size - _currentOffset;

        void* alignedPtr = std::align( alignment, size, currentPtr, space );

        if ( !alignedPtr )
        {
            this->unlock();
            return nullptr;
        }

        _currentOffset = static_cast<char*>( alignedPtr ) - static_cast<char*>( _backing.ptr ) + size;

        this->unlock();
        return alignedPtr;
    }

    void free( void* ptr ) override {
        AXION_LOG_ASSERT( true, Logger::Module::Common, "Linear Allocator does not support free()" )
        (void)ptr;
    }

    void reset() override {
        this->lock();
        _currentOffset = 0;
        this->unlock();
    }

    uint getUsedSize() const override {
        this->lock();
        uint used = _currentOffset;
        this->unlock();
        return used;
    }

    uint getTotalSize() const override { return _backing.size; }

private:
    VMemView _backing;
    uint       _currentOffset;
};

using LockedLinearAllocator = LinearAllocator<MutexLockPolicy>;


} // namespace Memory
AXION_NAMESPACE_END