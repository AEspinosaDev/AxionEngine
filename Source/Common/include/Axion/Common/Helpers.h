#pragma once
#include "Axion/Common/Defines.h"
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

constexpr uint alignu( uint value, uint alignment ) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
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
        static ulong                              frameCounter   = 0;
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