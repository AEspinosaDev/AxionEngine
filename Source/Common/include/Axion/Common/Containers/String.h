#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

/**
 * @brief String View. For now, just a wrapper of STL. WIP.
 * Lightweight, stack allocated.
 *
 */
using StringView = std::string_view;

/**
 * @brief A simple fixed-size string class that can be used in performance-critical paths where heap allocations are undesirable.
 * It provides basic string functionality while ensuring that the entire string data is stored inline, avoiding dynamic memory allocation.
 * Support for STL string_view allows for easy interoperability with standard string types without copying data.
 */
template <u32 N>
class FixedString
{
public:
    FixedString() { _data[0] = '\0'; }
    FixedString( const char* str ) {
        *this = StringView( str );
    }
    FixedString( StringView view ) {
        *this = view;
    }
    operator StringView() const { return { _data }; }

    FixedString& operator=( StringView view ) {
        u32 safeLen = (u32)std::min( (size_t)view.length(), (size_t)N - 1 );
        memcpy( _data, view.data(), safeLen );

        // Nos aseguramos de que el último byte sea CERO
        _data[safeLen] = '\0';
        return *this;
    }
    FixedString& operator=( const char* str ) {
        return *this = StringView( str );
    }

    bool operator==( StringView other ) const {
        return StringView( *this ) == other;
    }
    bool operator!=( StringView other ) const {
        return !( StringView( *this ) == other );
    }

    FixedString& operator+=( StringView view ) {
        u32 currentLen     = size();
        u32 remainingSpace = ( N - 1 ) - currentLen;
        u32 appendLen      = (u32)std::min( (size_t)view.length(), (size_t)remainingSpace );

        if ( appendLen > 0 )
        {
            memcpy( _data + currentLen, view.data(), appendLen );
            _data[currentLen + appendLen] = '\0';
        }
        return *this;
    }

    FixedString operator+( StringView view ) const {
        FixedString result = *this;
        result += view;
        return result;
    }

    const char* cstr() const { return _data; }
    u32         size() const { return (u32)strlen( _data ); }
    u32         capacity() const { return N; }
    bool        empty() const { return _data[0] == '\0'; }
    void        clear() { _data[0] = '\0'; }

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

template <u32 N>
inline StringView format_as( const FixedString<N>& str ) {
    return StringView( str );
}

AXION_NAMESPACE_END

namespace std {
template <u32 N>
struct hash<Axion::FixedString<N>> {
    std::size_t operator()( const Axion::FixedString<N>& str ) const noexcept {
        return std::hash<std::string_view> {}( std::string_view( str ) );
    }
};
} // namespace std