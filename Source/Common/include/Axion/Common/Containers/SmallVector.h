#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Memory/Allocators/IAllocator.h>
#include <initializer_list>
#include <utility>

AXION_NAMESPACE_BEGIN

constexpr u64 DEFAULT_STACK_BYTES = 64;

/***
 * @brief A hybrid dynamic array that uses internal stack storage for small capacities 
 * to avoid heap allocation overhead. It automatically spills to the heap if 
 * the element count exceeds the stack capacity.
 */
template <typename T,
          u64 N            = ( DEFAULT_STACK_BYTES < sizeof( T ) ) ? 1 : ( DEFAULT_STACK_BYTES / sizeof( T ) ),
          typename TPolicy = NoLockPolicy>
class SmallVector : public TPolicy
{
public:
    SmallVector()
        : _base( reinterpret_cast<T*>( _stackData ) )
        , _size( 0 )
        , _capacity( N ) {}

    SmallVector( const T& item )
        : SmallVector() {
        for ( u64 i = 0; i < N; ++i )
            pushBack( item );
    }

    SmallVector( std::initializer_list<T> list )
        : SmallVector() {
        if ( list.size() > N )
            reserve( list.size() );
        for ( const auto& item : list )
            pushBack( item );
    }

    SmallVector( Memory::IAllocator* allocator )
        : _base( reinterpret_cast<T*>( _stackData ) )
        , _size( 0 )
        , _capacity( N )
        , _allocator( allocator ) {}

    SmallVector( const T& item, Memory::IAllocator* allocator )
        : SmallVector( allocator ) {
        for ( u64 i = 0; i < N; ++i )
            pushBack( item );
    }

    // Copy Constructor (Deep Copy)
    SmallVector( const SmallVector& other )
        : _base( reinterpret_cast<T*>( _stackData ) )
        , _size( 0 )
        , _capacity( N )
        , _allocator( other._allocator ) {
        reserve( other._size );
        for ( u64 i = 0; i < other._size; ++i )
        {
            new ( _base + i ) T( other._base[i] );
        }
        _size = other._size;
    }

    // Copy Assignment Operator
    SmallVector& operator=( const SmallVector& other ) {
        if ( this == &other )
            return *this;

        this->lock();
        clear();

        _allocator = other._allocator;
        reserve( other._size );

        for ( u64 i = 0; i < other._size; ++i )
        {
            new ( _base + i ) T( other._base[i] );
        }
        _size = other._size;

        this->unlock();
        return *this;
    }

    // Move Constructor
    SmallVector( SmallVector&& other ) noexcept
        : _base( reinterpret_cast<T*>( _stackData ) )
        , _size( 0 )
        , _capacity( N )
        , _allocator( other._allocator ) {
        if ( other.isUsingStack() )
        {
            for ( u64 i = 0; i < other._size; ++i )
            {
                new ( _base + i ) T( std::move( other._base[i] ) );
                other._base[i].~T(); 
            }
            _size       = other._size;
            other._size = 0;
        } else
        {
            _base     = other._base;
            _size     = other._size;
            _capacity = other._capacity;

            other._base     = reinterpret_cast<T*>( other._stackData );
            other._size     = 0;
            other._capacity = N;
        }
    }

    // Move Assignment Operator
    SmallVector& operator=( SmallVector&& other ) noexcept {
        if ( this == &other )
            return *this;

        this->lock();
        clear();

        _allocator = other._allocator;

        if ( other.isUsingStack() )
        {
            // IMPORTANT FIX: Ensure capacity exists before moving elements.
            reserve( other._size ); 
            
            for ( u64 i = 0; i < other._size; ++i )
            {
                new ( _base + i ) T( std::move( other._base[i] ) );
                other._base[i].~T();
            }
            _size       = other._size;
            other._size = 0;
        } else
        {
            if ( !isUsingStack() )
            {
                if ( !_allocator )
                    ::operator delete( _base );
                else
                    _allocator->free( _base );
            }

            _base     = other._base;
            _size     = other._size;
            _capacity = other._capacity;

            other._base     = reinterpret_cast<T*>( other._stackData );
            other._size     = 0;
            other._capacity = N;
        }

        this->unlock();
        return *this;
    }

    ~SmallVector() {
        clear();
        if ( !isUsingStack() )
        {
            if ( !_allocator )
                ::operator delete( _base );
            else
                _allocator->free( _base );
        }
    }

    void resize( u64 newSize ) {
        this->lock();
        if ( newSize < _size )
        {
            for ( u64 i = newSize; i < _size; ++i )
                _base[i].~T();
        } else if ( newSize > _size )
        {
            if ( newSize > _capacity )
                reserve( newSize );
            for ( u64 i = _size; i < newSize; ++i )
                new ( _base + i ) T();
        }
        _size = newSize;
        this->unlock();
    }

    void resize( u64 newSize, const T& fillValue ) {
        this->lock();
        if ( newSize < _size )
        {
            for ( u64 i = newSize; i < _size; ++i )
                _base[i].~T();
        } else if ( newSize > _size )
        {
            if ( newSize > _capacity )
                reserve( newSize );
            for ( u64 i = _size; i < newSize; ++i )
                new ( _base + i ) T( fillValue );
        }
        _size = newSize;
        this->unlock();
    }

    void pushBack( const T& item ) {
        this->lock();
        if ( _size >= _capacity )
        {
            reserve( _capacity > 0 ? _capacity * 2 : 1 );
        }
        new ( _base + _size++ ) T( item );
        this->unlock();
    }

    void pushBack( T&& item ) {
        this->lock();
        if ( _size >= _capacity )
        {
            reserve( _capacity > 0 ? _capacity * 2 : 1 );
        }
        new ( _base + _size++ ) T( std::move( item ) );
        this->unlock();
    }

    void popBack() {
        this->lock();
        if ( _size > 0 )
        {
            _size--;
            _base[_size].~T();
        }
        this->unlock();
    }

    void removeAt( u64 index ) {
        this->lock();
        if ( index < _size )
        {
            for ( u64 i = index; i < _size - 1; ++i )
                _base[i] = std::move( _base[i + 1] );

            _size--;
            _base[_size].~T();
        }
        this->unlock();
    }

    void removeAtUnordered( u64 index ) {
        this->lock();
        if ( index < _size )
        {
            if ( index != _size - 1 )
                _base[index] = std::move( _base[_size - 1] );
            _size--;
            _base[_size].~T();
        }
        this->unlock();
    }

    void clear() {
        this->lock();
        for ( u64 i = 0; i < _size; ++i )
            _base[i].~T();
        _size = 0;
        this->unlock();
    }

    u64  size() const { return _size; }
    bool isEmpty() const { return _size == 0; }
    u64  capacity() const { return _capacity; }

    T&       first() { return _base[0]; }
    const T& first() const { return _base[0]; }

    T&       last() { return _base[_size - 1]; }
    const T& last() const { return _base[_size - 1]; }

    T* data() { return _base; }
    const T* data() const { return _base; }

    T&       operator[]( u64 index ) { return _base[index]; }
    const T& operator[]( u64 index ) const { return _base[index]; }

    T* begin() { return _base; }
    const T* begin() const { return _base; }
    T* end() { return _base + _size; }
    const T* end() const { return _base + _size; }

private:
    bool isUsingStack() const { return _base == reinterpret_cast<const T*>( _stackData ); }
    
    void reserve( u64 capacity ) {
        if ( capacity <= _capacity )
            return;

        this->lock();

        T* newBase = !_allocator ? static_cast<T*>( ::operator new( sizeof( T ) * capacity ) )
                                 : static_cast<T*>( _allocator->allocate( sizeof( T ) * capacity, alignof( T ) ) );
        for ( u64 i = 0; i < _size; ++i )
        {
            new ( newBase + i ) T( std::move( _base[i] ) );
            _base[i].~T();
        }

        if ( !isUsingStack() )
        {
            if ( !_allocator )
                ::operator delete( _base );
            else
                _allocator->free( _base );
        }
        _base     = newBase;
        _capacity = capacity;
        this->unlock();
    }

    alignas( T ) byte _stackData[N * sizeof( T )];

    T* _base      = nullptr;
    u64                 _size      = 0;
    u64                 _capacity  = 0;
    Memory::IAllocator* _allocator = nullptr;
};

AXION_NAMESPACE_END