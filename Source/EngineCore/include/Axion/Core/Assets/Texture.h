#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Graphics/Defines.h>
#include <Axion/Core/Assets/Defines.h>
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class AssetManager;

class Texture
{
public:
    struct SamplerDescription {
        Graphics::Filter      filter      = Graphics::Filter::Linear;
        Graphics::AddressMode addressMode = Graphics::AddressMode::Repeat;
        bool                  anisotropic = true;
        uchar                 mipLevels   = 1;
    };

    Texture()                     = delete;
    Texture( const Texture& )     = delete;
    Texture( Texture&& ) noexcept = default;

    [[nodiscard]] const std::string& getName() const { return _name; }
    [[nodiscard]] Extent3D           getSize() const { return _size; }
    [[nodiscard]] uint               getChannels() const { return _channels; }
    [[nodiscard]] bool               isHDR() const { return _isHDR; }
    [[nodiscard]] TextureType        getType() const { return _type; }
    [[nodiscard]] TextureFormat      getFormat() const { return _format; }
    [[nodiscard]] TexturePrecision   getPrecision() const { return _precision; }

    [[nodiscard]] Graphics::Format getGPUFormat() const { return _gpuFormat; }

    [[nodiscard]] const void* getPixels() const {
        if ( std::holds_alternative<std::vector<float>>( _pixels ) )
            return std::get<std::vector<float>>( _pixels ).data();
        return std::get<std::vector<uchar>>( _pixels ).data();
    }

    [[nodiscard]] size_t getSizeBytes() const {
        return _sizeBytes;
    }

    [[nodiscard]] const SamplerDescription& getSamplerDesc() const { return _samplerDesc; }
    void                                    setSamplerDesc( const SamplerDescription& desc ) { _samplerDesc = desc; }

private:
    friend class AssetManager;

    Texture( std::string name )
        : _name( std::move( name ) ) {}

    AXION_FORCE_INLINE void setData( Extent3D&&                         size,
                                     uint                               c,
                                     bool                               hdr,
                                     std::variant<std::vector<uchar>,
                                                  std::vector<float>>&& data,
                                     TextureFormat                      fmt,
                                     TexturePrecision                   precision,
                                     TextureType                        type ) {
        _size      = std::move( size );
        _type      = type;
        _precision = precision;
        _channels  = c;
        _isHDR     = hdr;
        _pixels    = std::move( data );
        _format    = fmt;
        _sizeBytes = computeSizeInBytes();
        _gpuFormat = getRecommendedGPUFormat( _format, _precision, _channels );
    }

    AXION_FORCE_INLINE size_t computeSizeInBytes() const {
        if ( _isHDR )
            return std::get<std::vector<float>>( _pixels ).size() * sizeof( float );
        return std::get<std::vector<unsigned char>>( _pixels ).size() * sizeof( unsigned char );
    }

    std::string      _name;
    Extent3D         _size;
    uint             _channels  = 0;
    bool             _isHDR     = false;
    TextureType      _type      = TextureType::Texture2D;
    TextureFormat    _format    = TextureFormat::Gamma;
    TexturePrecision _precision = TexturePrecision::U8;

    Graphics::Format _gpuFormat = Graphics::Format::UNKNOWN;

    SamplerDescription _samplerDesc;

    std::variant<std::vector<uchar>, std::vector<float>> _pixels;
    size_t                                               _sizeBytes = 0;
};

typedef Texture::SamplerDescription SamplerDesc;

} // namespace Core::Assets

AXION_NAMESPACE_END