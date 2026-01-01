#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Graphics/Defines.h>
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

struct Texture {
    std::string                                                  name;
    Extent3D                                                     size;
    uint                                                         channels = 0;
    bool                                                         isHDR    = false; // true = float (32 bit), false = uchar (8 bit)
    std::variant<std::vector<unsigned char>, std::vector<float>> pixels;
    Graphics::Format                                             format;
    Graphics::TextureDimension                                   dimension = Graphics::TextureDimension::Texture2D;

    struct Sampler {
        uint                  mipCount          = 1;
        bool                  anisotropicFilter = false;
        Graphics::AddressMode addressMode       = Graphics::AddressMode::Clamp;
        Graphics::Filter      filterMode        = Graphics::Filter::Linear;
    };

    Sampler sampler {};

    const void* getData() const {
        if ( isHDR )
            return std::get<std::vector<float>>( pixels ).data();
        return std::get<std::vector<unsigned char>>( pixels ).data();
    }

    size_t getSizeInBytes() const {
        if ( isHDR )
            return std::get<std::vector<float>>( pixels ).size() * sizeof( float );
        return std::get<std::vector<unsigned char>>( pixels ).size() * sizeof( unsigned char );
    }
};

} // namespace Core::Assets

AXION_NAMESPACE_END