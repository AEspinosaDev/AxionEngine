#pragma once
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

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// STL Headers
#include <chrono>
#include <iostream>
#include <memory>

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

#define DEFINE_SHARED_PTR_FOR_TYPE( type, clean ) \
    class type;                                   \
    typedef std::shared_ptr<type> clean##Ptr;

#define NEW_S( type ) \
    std::make_shared<type>

#define DEFINE_UNIQUE_PTR_FOR_TYPE( type, clean ) \
    class type;                                   \
    typedef std::unique_ptr<type> clean##Ptr;

#define NEW_U( type ) \
    std::make_unique<type>

#define AUTO_VAL 0xffffffff

#if defined(_MSC_VER)
    // Visual Studio (MSVC)
    #define AXION_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    // GCC y Clang
    #define AXION_FORCE_INLINE __attribute__((always_inline)) inline
#else
    // Fallback estándar
    #define AXION_FORCE_INLINE inline
#endif

// ---------------------------------------------------------------------------
// Handle Data Definitions
// ---------------------------------------------------------------------------

typedef unsigned long long ulong;
typedef unsigned int       uint;
typedef unsigned short     ushort;
typedef unsigned char      uchar;

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
    uint width { 0 };
    uint height { 0 };

    inline bool operator==( const Extent2D o ) const {
        return width == o.width && height == o.height;
    }
    inline bool operator!=( const Extent2D o ) const {
        return width != o.width || height != o.height;
    }
    Extent3D    to3D() const;
};
struct Extent3D {
    uint width { 0 };
    uint height { 0 };
    uint depth { 0 };
    
    inline bool operator==( const Extent3D& o ) const {
        return width == o.width && height == o.height && depth == o.depth;
    }
    inline bool operator!=( const Extent3D& o ) const {
        return width != o.width || height != o.height || depth != o.depth;
    }
    Extent2D    to2D() const;
};
struct Position2D {
    uint x { 0 };
    uint y { 0 };

    inline bool operator==( const Position2D& o ) const {
        return x == o.x && y == o.y;
    }
    inline bool operator!=( const Position2D& o ) const {
        return x != o.x && y != o.y;
    }
};

AXION_NAMESPACE_END