#pragma once
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <atomic>
#include <utility>

AXION_NAMESPACE_BEGIN
namespace Memory {

// SharedPtr Control Policy
template <typename T>
struct AtomicControlPolicy {
    std::atomic<uint> refCount;
    IAllocator*       allocator = nullptr;
    alignas( T ) unsigned char payload[sizeof( T )];

    uint increment() { return refCount.fetch_add( 1, std::memory_order_relaxed ) + 1; }
    void store( uint value = 0 ) { refCount.store( value, std::memory_order_relaxed ); }
    uint decrement() { return refCount.fetch_sub( 1, std::memory_order_relaxed ) - 1; }
    uint getCount() { return refCount.load( std::memory_order_relaxed ); }

    T* getPayload() { return reinterpret_cast<T*>( payload ); }
};

template <typename T>
struct ControlPolicy {
    uint        refCount  = 0;
    IAllocator* allocator = nullptr;
    alignas( T ) unsigned char payload[sizeof( T )];

    void store( uint value = 0 ) { refCount = value; }
    uint increment() { return ++refCount; }
    uint decrement() { return --refCount; }
    uint getCount() { return refCount; }

    T* getPayload() { return reinterpret_cast<T*>( payload ); }
};

/**
 * SharedPtr
 * Reference counted ownership.
 */
template <typename T, typename TControlPolicy = AtomicControlPolicy<T>>
class SharedPtr
{
public:
    SharedPtr()
        : _ptr( nullptr )
        , _controlPolicy( nullptr ) {}

    SharedPtr( T* ptr, TControlPolicy* policy )
        : _ptr( ptr )
        , _controlPolicy( policy ) {
        if ( _controlPolicy )
            _controlPolicy->increment();
    }

    ~SharedPtr() { release(); }

    SharedPtr( const SharedPtr& other )
        : _ptr( other._ptr )
        , _controlPolicy( other._controlPolicy ) {
        if ( _controlPolicy )
            _controlPolicy->increment();
    }

    SharedPtr& operator=( const SharedPtr& other ) {
        if ( this != &other )
        {
            release();
            _ptr           = other._ptr;
            _controlPolicy = other._controlPolicy;
            if ( _controlPolicy )
                _controlPolicy->increment();
        }
        return *this;
    }

    SharedPtr( SharedPtr&& other ) noexcept
        : _ptr( other._ptr )
        , _controlPolicy( other._controlPolicy ) {
        other._ptr           = nullptr;
        other._controlPolicy = nullptr;
    }

    template <typename U, typename UControlPolicy>
    SharedPtr( const SharedPtr<U, UControlPolicy>& other )
        : _ptr( other._ptr )
        , _controlPolicy( reinterpret_cast<TControlPolicy*>( other._controlPolicy ) ) {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible pointer types." );
        if ( _controlPolicy )
            _controlPolicy->increment();
    }

    template <typename U, typename UControlPolicy>
    SharedPtr( SharedPtr<U, UControlPolicy>&& other ) noexcept
        : _ptr( other._ptr )
        , _controlPolicy( reinterpret_cast<TControlPolicy*>( other._controlPolicy ) ) {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible pointer types." );
        other._ptr           = nullptr;
        other._controlPolicy = nullptr;
    }

    template <typename U, typename UControlPolicy>
    SharedPtr& operator=( const SharedPtr<U, UControlPolicy>& other ) {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible pointer types." );
        if ( (void*)this != (void*)&other )
        {
            release();
            _ptr           = other._ptr;
            _controlPolicy = reinterpret_cast<TControlPolicy*>( other._controlPolicy );
            if ( _controlPolicy )
                _controlPolicy->increment();
        }
        return *this;
    }

    template <typename U, typename UControlPolicy>
    SharedPtr( T* ptr, const SharedPtr<U, UControlPolicy>& other ) noexcept
        : _ptr( ptr )
        , _controlPolicy( reinterpret_cast<TControlPolicy*>( other._controlPolicy ) ) {
        if ( _controlPolicy )
            _controlPolicy->increment();
    }

    template <typename U, typename UControlPolicy>
    SharedPtr& operator=( SharedPtr<U, UControlPolicy>&& other ) noexcept {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible pointer types." );
        if ( (void*)this != (void*)&other )
        {
            release();
            _ptr                 = other._ptr;
            _controlPolicy       = reinterpret_cast<TControlPolicy*>( other._controlPolicy );
            other._ptr           = nullptr;
            other._controlPolicy = nullptr;
        }
        return *this;
    }

    SharedPtr& operator=( SharedPtr&& other ) noexcept {
        if ( this != &other )
        {
            release();
            _ptr                 = other._ptr;
            _controlPolicy       = other._controlPolicy;
            other._ptr           = nullptr;
            other._controlPolicy = nullptr;
        }
        return *this;
    }

    T*       operator->() const { return _ptr; }
    T&       operator*() const { return *_ptr; }
    T*       get() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

    uint useCount() const {
        return _controlPolicy ? _controlPolicy->getCount() : 0;
    }

private:
    void release() {
        if ( _controlPolicy )
        {
            if ( _controlPolicy->decrement() == 0 )
            {
                if ( _ptr )
                    _ptr->~T();

                IAllocator* alloc = _controlPolicy->allocator;
                _controlPolicy->~TControlPolicy();
                if ( alloc )
                    alloc->free( _controlPolicy );
                else
                    delete ( _controlPolicy );
            }
        }
        _ptr           = nullptr;
        _controlPolicy = nullptr;
    }

    T*              _ptr;
    TControlPolicy* _controlPolicy;
};

template <typename T, typename TControlPolicy = AtomicControlPolicy<T>, typename... Args>
SharedPtr<T> makeSharedWith( IAllocator* allocator, Args&&... args ) {
    if ( allocator )
    {
        void* mem = allocator->allocate( sizeof( TControlPolicy ), alignof( TControlPolicy ) );

        auto* block = new ( mem ) TControlPolicy();
        block->refCount.store( 0 );
        block->allocator = allocator;

        T* obj = new ( block->getPayload() ) T( std::forward<Args>( args )... );
        return SharedPtr<T>( obj, block );

    } else
    {
        auto* block = new TControlPolicy();
        block->refCount.store( 0 );

        T* obj = new ( block->getPayload() ) T( std::forward<Args>( args )... );
        return SharedPtr<T>( obj, block );
    }
}

template <typename T, typename TControlPolicy = AtomicControlPolicy<T>, typename... Args>
SharedPtr<T> makeShared( Args&&... args ) {

    auto* block = new TControlPolicy();
    block->refCount.store( 0 );

    T* obj = new ( block->getPayload() ) T( std::forward<Args>( args )... );
    return SharedPtr<T>( obj, block );
}

} // namespace Memory
AXION_NAMESPACE_END

#define DEFINE_SHARED_PTR_FOR_TYPE( type, clean ) \
    class type;                                   \
    using clean##SharedPtr = Axion::Memory::SharedPtr<type>;