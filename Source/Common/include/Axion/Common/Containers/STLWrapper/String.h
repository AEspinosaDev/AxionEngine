#pragma once
#include <Axion/Common/Memory/STLAdapter.h>
#include <string>

AXION_NAMESPACE_BEGIN

namespace STLW {

// String wrapper
class String : public std::basic_string<char, std::char_traits<char>, Memory::STLAdapter<char>>
{
public:
    using Base = std::basic_string<char, std::char_traits<char>, Memory::STLAdapter<char>>;
    using Base::Base;

    explicit String( Memory::IAllocator* allocator )
        : Base( Memory::STLAdapter<char>( allocator ) ) {}

    String( const char* str, Memory::IAllocator* allocator )
        : Base( str, Memory::STLAdapter<char>( allocator ) ) {}
    explicit String()
        : Base( Memory::STLAdapter<char>( nullptr ) ) {}
};

} // namespace STLW

AXION_NAMESPACE_END