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
    using Base::operator=;

    explicit String( Memory::IAllocator* allocator )
        : Base( Memory::STLAdapter<char>( allocator ) ) {}

    String( const char* str, Memory::IAllocator* allocator )
        : Base( str, Memory::STLAdapter<char>( allocator ) ) {}
    String()
        : Base( Memory::STLAdapter<char>( nullptr ) ) {}

    String( const Base& other )
        : Base( other ) {}

    String( Base&& other ) noexcept
        : Base( std::move( other ) ) {}

    String& operator=( const Base& other ) {
        Base::operator=( other );
        return *this;
    }
    String& operator=( Base&& other ) noexcept {
        Base::operator=( std::move( other ) );
        return *this;
    }
};

} // namespace STLW

AXION_NAMESPACE_END