#pragma once
#include "Axion/Common/Common.h"
#include <variant>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Helpers {

inline void hashCombine( size_t& seed, size_t value ) {
    seed ^= value + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

template <typename T>
constexpr T alignUp( T value, T alignment ) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

/**
 * Only workf with multiple of 2
 */
constexpr u32 alignubits( u32 value, u32 alignment ) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}
constexpr u32 alignu( u32 value, u32 alignment ) {
    if ( alignment == 0 )
        return value;
    u32 remainder = value % alignment;
    if ( remainder == 0 )
        return value;
    return value + ( alignment - remainder );
}


constexpr u64 safeAlign( u64 current, u64 align ) {
    if ( align <= 1 )
        return current;

    // FAST TRACK: Si es potencia de 2 (ej: 256, 16, 4)
    // (align & (align - 1)) == 0 es el truco estándar para checkear power-of-2
    if ( ( align & ( align - 1 ) ) == 0 )
        return ( current + ( align - 1 ) ) & ~( align - 1 );

    // SAFE TRACK: Aritmética para tamaños raros (ej: 144, 80)
    u64 remainder = current % align;
    if ( remainder == 0 )
        return current;
    return current + ( align - remainder );
}

class Clock
{
public:
    struct Info {
        double fps;
        double totalTime;
        double deltaTime;
    };

    static Info tick() {
        static u64                              frameCounter   = 0;
        static double                             elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto                               t0 = clock.now();

        frameCounter++;
        auto t1        = clock.now();
        auto deltaTime = t1 - t0;
        t0             = t1;

        double dt = deltaTime.count() * 1e-9;
        elapsedSeconds += dt;
        return { .fps       = frameCounter / elapsedSeconds,
                 .totalTime = elapsedSeconds,
                 .deltaTime = dt };
    }
};

} // namespace Helpers
AXION_NAMESPACE_END