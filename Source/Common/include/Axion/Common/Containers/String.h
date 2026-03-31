#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

/**
 * @brief A simple fixed-size string class that can be used in performance-critical paths where heap allocations are undesirable.
 * It provides basic string functionality while ensuring that the entire string data is stored inline, avoiding dynamic memory allocation.
 * Support for STL string_view allows for easy interoperability with standard string types without copying data.
 */
template <uint N>
class FixedString
{
public:
    FixedString() { _data[0] = '\0'; }
    FixedString( const char* str ) {
        *this = std::string_view( str );
    }
    FixedString( std::string_view view ) {
        *this = view;
    }
    operator std::string_view() const { return { _data }; }

    FixedString& operator=( std::string_view view ) {
        uint safeLen = (uint)std::min( (size_t)view.length(), (size_t)N - 1 );
        memcpy( _data, view.data(), safeLen );

        // Nos aseguramos de que el último byte sea CERO
        _data[safeLen] = '\0';
        return *this;
    }
    bool operator==( std::string_view other ) const {
        return std::string_view( *this ) == other;
    }
    bool operator!=( std::string_view other ) const {
        return !( std::string_view( *this ) == other );
    }

    const char* c_str() const { return _data; }
    uint        size() const { return (uint)strlen( _data ); }
    uint        capacity() const { return N; }
    bool        empty() const { return _data[0] == '\0'; }

    // Iterable
    char*       begin() { return _data; }
    const char* begin() const { return _data; }
    char*       end() { return _data + size(); }
    const char* end() const { return _data + size(); }

private:
    char _data[N] {};
};

using String32  = FixedString<32>;
using String64  = FixedString<64>;
using String128 = FixedString<128>;

AXION_NAMESPACE_END