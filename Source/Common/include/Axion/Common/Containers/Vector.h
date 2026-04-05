#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Memory/Allocators/IAllocator.h>

AXION_NAMESPACE_BEGIN

constexpr u64 DEFAULT_VECTOR_HEAP_CAPACITY = 4;

/***
 * @brief A simple dynamic array implementation that provides basic functionalities such as dynamic resizing, element access, and iteration. It is designed to be a lightweight alternative to std::vector, with a focus on performance and minimal overhead.
 * Note: This implementation does not handle copy/move semantics for the elements, so it is recommended to use it with trivially copyable types or to implement proper copy/move constructors and assignment operators for non-trivial types.
 */
template <typename T, typename TPolicy = NoLockPolicy>
class Vector : public TPolicy
{
public:
    // No Allocator constructor, defaults to using the global operator new/delete
    Vector() = default;
    Vector( u64 capacity ) { reserve( capacity ); }
    Vector( u64 capacity, const T& item ) {
        reserve( capacity );
        for ( u64 i = 0; i < capacity; ++i )
            pushBack( item );
    }
    Vector( std::initializer_list<T> list ) {
        reserve( list.size() );
        for ( const auto& item : list )
            pushBack( item );
    }
    // Allocator constructor
    Vector( Memory::IAllocator* allocator )
        : _allocator( allocator ) {
        reserve( DEFAULT_VECTOR_HEAP_CAPACITY );
    }
    Vector( u64 capacity, Memory::IAllocator* allocator )
        : _allocator( allocator ) {
        reserve( capacity );
    }
    Vector( u64 capacity, const T& item, Memory::IAllocator* allocator )
        : _allocator( allocator ) {
        reserve( capacity );
        for ( u64 i = 0; i < capacity; ++i )
            pushBack( item );
    }

    Vector( const Vector& other )
        : _allocator( other._allocator ) {
        reserve( other._capacity );
        for ( u64 i = 0; i < other._size; ++i )
            pushBack( other._base[i] );
    }
    Vector& operator=( const Vector& other ) {
        if ( this == &other )
            return *this;
        this->lock();
        clear();
        _allocator = other._allocator;
        reserve( other._capacity );
        for ( u64 i = 0; i < other._size; ++i )
            new ( &_base[_size++] ) T( other._base[i] );
        this->unlock();
        return *this;
    }

    Vector( Vector&& other ) noexcept
        : _base( other._base )
        , _size( other._size )
        , _capacity( other._capacity )
        , _allocator( other._allocator ) {
        other._base     = nullptr;
        other._size     = 0;
        other._capacity = 0;
    }

    Vector& operator=( Vector&& other ) noexcept {
        if ( this == &other )
            return *this;
        this->lock();
        clear();
        if ( !_allocator )
        {
            ::operator delete( _base );
        } else
        {
            _allocator->free( _base );
        }

        _base      = other._base;
        _size      = other._size;
        _capacity  = other._capacity;
        _allocator = other._allocator;

        other._base     = nullptr;
        other._size     = 0;
        other._capacity = 0;
        this->unlock();
        return *this;
    }

    ~Vector() {
        clear();
        if ( _base )
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
                new ( &_base[i] ) T();
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
                new ( &_base[i] ) T( fillValue );
        }
        _size = newSize;
        this->unlock();
    }

    void reserve( u64 capacity ) {
        if ( capacity <= _capacity )
            return;

        this->lock();
        T* newBase = !_allocator ? static_cast<T*>( ::operator new( sizeof( T ) * capacity ) )
                                 : static_cast<T*>( _allocator->allocate( sizeof( T ) * capacity, alignof( T ) ) );
        for ( u64 i = 0; i < _size; ++i )
        {
            new ( &newBase[i] ) T( std::move( _base[i] ) );
            _base[i].~T();
        }

        if ( _base )
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

    void pushBack( const T& item ) {
        this->lock();
        if ( _size >= _capacity )
            reserve( _capacity > 0 ? _capacity * 2 : DEFAULT_VECTOR_HEAP_CAPACITY );
        new ( &_base[_size++] ) T( item );
        this->unlock();
    }

    void pushBack( T&& item ) {
        this->lock();
        if ( _size >= _capacity )
            reserve( _capacity > 0 ? _capacity * 2 : DEFAULT_VECTOR_HEAP_CAPACITY );
        new ( &_base[_size++] ) T( std::move( item ) );
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

    // Very costly O(n) operation, since it shifts memory,
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

    // Unordered removal (Swap & Pop). Extremely fast O(1) time.
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

    T*       data() { return _base; }
    const T* data() const { return _base; }

    T&       operator[]( u64 index ) { return _base[index]; }
    const T& operator[]( u64 index ) const { return _base[index]; }

    T*       begin() { return _base; }
    const T* begin() const { return _base; }
    T*       end() { return _base + _size; }
    const T* end() const { return _base + _size; }

private:
    T*                  _base      = nullptr;
    u64                 _size      = 0;
    u64                 _capacity  = 0;
    Memory::IAllocator* _allocator = nullptr;
};

AXION_NAMESPACE_END
