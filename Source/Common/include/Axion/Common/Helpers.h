#pragma once
#include "Axion/Common/Defines.h"
#include <vector>
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Helpers {

inline void hashCombine( size_t& seed, size_t value ) {
    seed ^= value + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

template <typename T>
constexpr T alignUp( T value, T alignment ) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

struct ImageData {
    uint width = 0;
    uint height = 0;
    uint channels = 0;
    bool isHDR = false; // true = float (32 bit), false = uchar (8 bit)
    std::variant<std::vector<unsigned char>, std::vector<float>> pixels;

    const void* getData() const {
        if (isHDR) return std::get<std::vector<float>>(pixels).data();
        return std::get<std::vector<unsigned char>>(pixels).data();
    }
    
    size_t getSizeInBytes() const {
        if (isHDR) return std::get<std::vector<float>>(pixels).size() * sizeof(float);
        return std::get<std::vector<unsigned char>>(pixels).size() * sizeof(unsigned char);
    }
};

ImageData loadImage(const std::string& path, int forceChannels = 4, bool flipVertically = false) ;


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