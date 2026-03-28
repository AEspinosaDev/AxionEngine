#pragma once
#include <Axion/Common/Memory/STLAdapter.h>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace STLW {

// Vector wrapper
template <typename T>
class Vector : public std::vector<T, Memory::STLAdapter<T>>
{
public:
    using Base = std::vector<T, Memory::STLAdapter<T>>;
    using Base::Base;

    explicit Vector( Memory::IAllocator* allocator )
        : Base( Memory::STLAdapter<T>( allocator ) ) {}
    explicit Vector()
        : Base( Memory::STLAdapter<T>( nullptr ) ) {}
};

} // namespace STLW

AXION_NAMESPACE_END