#pragma once
#include <Axion/Common/Memory/Allocators/IAllocator.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

/**
 * OwnerPtr (Equivalent to std::unique_ptr)
 * Move-only semantics. Strictly owns the memory and the lifecycle of the object.
 */
template <typename T>
class OwnerPtr
{
    template <typename U>
    friend class OwnerPtr;

public:
    AXION_DISABLE_COPY( OwnerPtr )

    OwnerPtr()
        : _ptr( nullptr )
        , _allocator( nullptr ) {}

    OwnerPtr( T* ptr, IAllocator* allocator = nullptr )
        : _ptr( ptr )
        , _allocator( allocator ) {}

    ~OwnerPtr() { reset(); }

    OwnerPtr( OwnerPtr&& other ) noexcept
        : _ptr( other._ptr )
        , _allocator( other._allocator ) {
        other._ptr       = nullptr;
        other._allocator = nullptr;
    }

    template <typename U>
    OwnerPtr( OwnerPtr<U>&& other ) noexcept
        : _ptr( other._ptr )
        , _allocator( other._allocator ) {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible types in OwnerPtr move constructor." );
        other._ptr       = nullptr;
        other._allocator = nullptr;
    }

    template <typename U>
    OwnerPtr& operator=( OwnerPtr<U>&& other ) noexcept {
        static_assert( std::is_convertible_v<U*, T*>, "Incompatible types in OwnerPtr assignment." );

        if ( (void*)this != (void*)&other )
        {
            reset();

            _ptr       = other._ptr;
            _allocator = other._allocator;

            other._ptr       = nullptr;
            other._allocator = nullptr;
        }
        return *this;
    }

    OwnerPtr& operator=( OwnerPtr&& other ) noexcept {
        if ( this != &other )
        {
            reset();
            _ptr             = other._ptr;
            _allocator       = other._allocator;
            other._ptr       = nullptr;
            other._allocator = nullptr;
        }
        return *this;
    }

    T*       operator->() const { return _ptr; }
    T&       operator*() const { return *_ptr; }
    T*       get() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

    void reset( T* p = nullptr ) {
        if ( _ptr == p )
            return;

        if ( _ptr )
        {
            if ( _allocator )
            {
                _ptr->~T();
                _allocator->free( _ptr );
            } else
            {
                delete _ptr;
            }
        }

        _ptr = p;
    }

    T* release() {
        T* temp    = _ptr;
        _ptr       = nullptr;
        _allocator = nullptr;
        return temp;
    }

private:
    T*          _ptr       = nullptr;
    IAllocator* _allocator = nullptr;
};

// Factory Functions
// -----------------------------------------------------------------------------

template <typename T, typename... Args>
OwnerPtr<T> makeOwnedWith( IAllocator* allocator, Args&&... args ) {
    if ( allocator )
    {
        void* mem = allocator->allocate( sizeof( T ), alignof( T ) );
        T*    obj = new ( mem ) T( std::forward<Args>( args )... );
        return OwnerPtr<T>( obj, allocator );
    } else
    {
        T* obj = new T( std::forward<Args>( args )... );
        return OwnerPtr<T>( obj );
    }
}

template <typename T, typename... Args>
OwnerPtr<T> makeOwned( Args&&... args ) {
    T* obj = new T( std::forward<Args>( args )... );
    return OwnerPtr<T>( obj );
}

} // namespace Memory
AXION_NAMESPACE_END

#define DEFINE_OWNER_PTR_FOR_TYPE( type, clean ) \
    class type;                                  \
    using clean##OwnerPtr = Axion::Memory::OwnerPtr<type>;