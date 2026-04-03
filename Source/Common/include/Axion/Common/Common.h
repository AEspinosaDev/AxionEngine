#pragma once

#if defined( _WIN32 )

// Windows headers
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

#if defined( min )
#undef min
#endif

#if defined( max )
#undef max
#endif

#define AXION_DEBUG_BREAK() __debugbreak()

#elif defined( __linux__ )

#define AXION_DEBUG_BREAK() __builtin_trap()

#else
#error "Platform not supported!"
#endif

// STL Headers
#include <chrono>
#include <iostream>

// FMT
#include <fmt/core.h>
#include <fmt/format.h>

// ---------------------------------------------------------------------------
// Handle MACRO Definitions
// ---------------------------------------------------------------------------

#define AXION_NAMESPACE_BEGIN namespace Axion {
#define AXION_NAMESPACE_END }
#define USING_AXION_NAMESPACE using namespace Axion;

#if defined( _DEBUG ) || !defined( NDEBUG )
#define AXION_DEBUG
#endif
#define AXION_ENUM_CLASS_FLAG_OPERATORS( T )                                                                                \
    inline T  operator|( T a, T b ) { return T( uint32_t( a ) | uint32_t( b ) ); }                                          \
    inline T  operator&( T a, T b ) { return T( uint32_t( a ) & uint32_t( b ) ); } /* NOLINT(bugprone-macro-parentheses) */ \
    inline T  operator~( T a ) { return T( ~uint32_t( a ) ); }                     /* NOLINT(bugprone-macro-parentheses) */ \
    inline T& operator|=( T& a, T b ) {                                                                                     \
        a = a | b;                                                                                                          \
        return a;                                                                                                           \
    }                                                                                                                       \
    inline T& operator&=( T& a, T b ) {                                                                                     \
        a = a & b;                                                                                                          \
        return a;                                                                                                           \
    }                                                                                                                       \
    inline bool operator!( T a ) { return uint32_t( a ) == 0; }                                                             \
    inline bool operator==( T a, uint32_t b ) { return uint32_t( a ) == b; }                                                \
    inline bool operator!=( T a, uint32_t b ) { return uint32_t( a ) != b; }

#define AUTO_VAL 0xffffffff

#if defined( _MSC_VER )
// Visual Studio (MSVC)
#define AXION_FORCE_INLINE __forceinline
#elif defined( __GNUC__ ) || defined( __clang__ )
// GCC y Clang
#define AXION_FORCE_INLINE __attribute__( ( always_inline ) ) inline
#else
// Fallback estándar
#define AXION_FORCE_INLINE inline
#endif

#define AXION_UNUSED_PARAMETER( param ) ( (void)( param ) )

// ---------------------------------------------------------------------------
// Handle Move and Copy Semantics
// ---------------------------------------------------------------------------

#define AXION_DISABLE_COPY( TypeName )               \
    TypeName( const TypeName& )            = delete; \
    TypeName& operator=( const TypeName& ) = delete;

// Disables the move constructor and move assignment operator
#define AXION_DISABLE_MOVE( TypeName )          \
    TypeName( TypeName&& )            = delete; \
    TypeName& operator=( TypeName&& ) = delete;

// Disables both copy and move semantics
#define AXION_DISABLE_COPY_AND_MOVE( TypeName ) \
    AXION_DISABLE_COPY( TypeName )              \
    AXION_DISABLE_MOVE( TypeName )

// ---------------------------------------------------------------------------
// Handle Data Definitions
// ---------------------------------------------------------------------------

// Primitive types

using u64 = uint64_t; // Unsigned Int 64
using u32 = uint32_t; // Unsigned Int 32
using u16 = uint16_t; // Unsigned Int 16

using byte = uint8_t; // Byte (alias for uint8_t)

using s64 = int64_t;
using s32 = int32_t;
using s16 = int16_t;
using s8  = int8_t;

AXION_NAMESPACE_BEGIN

/// Simple exception class, which stores a human-readable error description
class AxionException : public std::runtime_error
{
public:
    template <typename... Args>
    AxionException( const char* fmt, const Args&... args )
        : std::runtime_error( fmt ) {
    }
};

struct Extent3D;
struct Extent2D {
    u32 width { 0 };
    u32 height { 0 };

    inline bool operator==( const Extent2D o ) const {
        return width == o.width && height == o.height;
    }
    inline bool operator!=( const Extent2D o ) const {
        return width != o.width || height != o.height;
    }
    Extent3D to3D() const;
};
struct Extent3D {
    u32 width { 0 };
    u32 height { 0 };
    u32 depth { 0 };

    inline bool operator==( const Extent3D& o ) const {
        return width == o.width && height == o.height && depth == o.depth;
    }
    inline bool operator!=( const Extent3D& o ) const {
        return width != o.width || height != o.height || depth != o.depth;
    }
    Extent2D to2D() const;
};
struct Position2D {
    u32 x { 0 };
    u32 y { 0 };

    inline bool operator==( const Position2D& o ) const {
        return x == o.x && y == o.y;
    }
    inline bool operator!=( const Position2D& o ) const {
        return x != o.x && y != o.y;
    }
};

AXION_NAMESPACE_END