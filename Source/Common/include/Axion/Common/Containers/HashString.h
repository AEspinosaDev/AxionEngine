#pragma once
#include "Axion/Common/Common.h"

#ifdef AXION_DEBUG
#include <string>
#endif

AXION_NAMESPACE_BEGIN

class HashString;
using StringID = HashString;

class HashString
{
public:
    constexpr HashString()
        : _hash( 0 ) {}

    template <u64 N>
    constexpr HashString( const byte ( &str )[N] )
        : _hash( byteString2Hash( str, N - 1 ) ) {}

    explicit HashString( const byte* str )
        : _hash( byteString2Hash( str ) ) {}

    constexpr u64 getHash() const { return _hash; }

    constexpr bool operator==( const HashString& other ) const { return _hash == other._hash; }
    constexpr bool operator!=( const HashString& other ) const { return _hash != other._hash; }

private:
    // FNV-1a 64-bit constants
    static constexpr u64 FNV_OFFSET_BASIS = 0xCBF29CE484222325ULL;
    static constexpr u64 FNV_PRIME        = 0x100000001B3ULL;

    // Compile-time eval
    static constexpr u64 byteString2Hash( const byte* str, u64 length ) {
        u64 hash = FNV_OFFSET_BASIS;
        for ( size_t i = 0; i < length; ++i )
        {
            hash ^= static_cast<u64>( str[i] );
            hash *= FNV_PRIME;
        }
        return hash;
    }

    // Runtime eval
    static u64 byteString2Hash( const byte* str ) {
        u64 hash = FNV_OFFSET_BASIS;
        while ( *str )
        {
            hash ^= static_cast<u64>( *str );
            hash *= FNV_PRIME;
            ++str;
        }
        return hash;
    }

    u64 _hash;
};

AXION_NAMESPACE_END