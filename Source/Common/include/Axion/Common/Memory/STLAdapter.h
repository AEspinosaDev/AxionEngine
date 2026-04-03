#pragma once
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <cstddef>
#include <type_traits>
#include <utility>

AXION_NAMESPACE_BEGIN
namespace Memory {

template <typename T>
class STLAdapter
{
public:
    // Required STL allocator typedefs
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = STLAdapter<U>;
    };

    IAllocator* _allocator = nullptr;

    STLAdapter() noexcept = default;
    explicit STLAdapter( IAllocator* allocator ) noexcept
        : _allocator( allocator ) {}
    template <typename U>
    STLAdapter( const STLAdapter<U>& other ) noexcept
        : _allocator( other._allocator ) {}

    T* allocate( std::size_t n ) {
        if ( !_allocator )
            return std::allocator<T>().allocate( n );

        void* ptr = _allocator->allocate( static_cast<u32>( n * sizeof( T ) ), alignof( T ) );
        return static_cast<T*>( ptr );
    }

    void deallocate( T* p, std::size_t n ) noexcept {
        if ( !_allocator )
        {
            std::allocator<T>().deallocate( p, n );
            return;
        }

        if ( p )
        {
            _allocator->free( p );
        }
    }

    // Equality operators are required.
    // Two stateful allocators are considered equal if they allocate from the same memory pool.
    template <typename U>
    bool operator==( const STLAdapter<U>& other ) const noexcept {
        return _allocator == other._allocator;
    }

    template <typename U>
    bool operator!=( const STLAdapter<U>& other ) const noexcept {
        return _allocator != other._allocator;
    }
};

} // namespace Memory
AXION_NAMESPACE_END