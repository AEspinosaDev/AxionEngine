#pragma once
#include <Axion/Common/Memory/STLAdapter.h>
#include <list>
#include <queue>

AXION_NAMESPACE_BEGIN

namespace STLW {

// List wrapper
template <typename T>
class List : public std::list<T, Memory::STLAdapter<T>>
{
public:
    using Base = std::list<T, Memory::STLAdapter<T>>;

    using Base::Base;

    explicit List( Memory::IAllocator* allocator )
        : Base( Memory::STLAdapter<T>( allocator ) ) {}
    explicit List()
        : Base( Memory::STLAdapter<T>( nullptr ) ) {}
};

// Deque wrapper
template <typename T>
class Deque : public std::deque<T, Memory::STLAdapter<T>>
{
public:
    using Base = std::deque<T, Memory::STLAdapter<T>>;
    using Base::Base;

    explicit Deque( Memory::IAllocator* allocator )
        : Base( Memory::STLAdapter<T>( allocator ) ) {}
    explicit Deque()
        : Base( Memory::STLAdapter<T>( nullptr ) ) {}
};

template <typename T, typename Container = Deque<T>>
class Queue : public std::queue<T, Container>
{
public:
    using Base = std::queue<T, Container>;
    using Base::Base;
    explicit Queue( Memory::IAllocator* allocator )
        : Base( Container( allocator ) ) {}
    explicit Queue()
        : Base( Container( nullptr ) ) {}
};

} // namespace STLW

AXION_NAMESPACE_END