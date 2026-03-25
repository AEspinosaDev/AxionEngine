#pragma once
#include <Axion/Common/Logging.h>
#include <Axion/Common/Memory/IAllocator.h>
#include <Axion/Common/Memory/MemoryManager.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename LockPolicy = NoLockPolicy>
class LinearAllocator : public IAllocator, public LockPolicy
{
public:
    LinearAllocator( uint capacity ) {
        _pool = VMemManager::virtualReserve( capacity );
        if ( !VMemManager::virtualCommit( _pool ) )
            throw AxionException( "Failed to commit virtual memory for LinearAllocator" );

        _currentOffset = 0;
    }

    ~LinearAllocator() override {
        VMemManager::virtualDecommit( _pool );
        VMemManager::virtualRelease( _pool );
    }

    void* allocate( uint size, uint alignment = 16 ) override {
        this->lock();

        if ( !_pool.isValid() )
        {
            this->unlock();
            return nullptr;
        }

        MemoryAddress poolStart = reinterpret_cast<MemoryAddress>( _pool.ptr );

        void* currentPtr = static_cast<void*>( poolStart + _currentOffset );
        uint  space      = _pool.size - _currentOffset;

        void* alignedPtr = std::align( alignment, size, currentPtr, space );

        if ( !alignedPtr )
        {
            this->unlock();
            return nullptr;
        }

        MemoryAddress alignedAddress = reinterpret_cast<MemoryAddress>( alignedPtr );
        _currentOffset               = static_cast<uint>( alignedAddress - poolStart ) + size;

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

    uint getTotalSize() const override { return _pool.size; }

private:
    VMemView _pool;
    uint     _currentOffset;
};

using LockedLinearAllocator = LinearAllocator<MutexLockPolicy>;

} // namespace Memory
AXION_NAMESPACE_END