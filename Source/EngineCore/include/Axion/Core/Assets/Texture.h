#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Core/Assets/Common.h>
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class AssetManager;

using TexturePixels = std::variant<std::vector<float>, std::vector<uchar>>;

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

    [[nodiscard]] std::shared_ptr<TexturePixels> getPixelsRef() const {
        return _pixels;
    }
    [[nodiscard]] const void* getPixels() const {
        if ( !_pixels )
            return nullptr;

        return std::visit( []( const auto& vec ) -> const void* {
            if ( vec.empty() )
                return nullptr;
            return vec.data();
        },
                           *_pixels );
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

    AXION_FORCE_INLINE void setData( Extent3D&&       size,
                                     uint             c,
                                     bool             hdr,
                                     TexturePixels&&  data,
                                     TextureFormat    fmt,
                                     TexturePrecision precision,
                                     TextureType      type ) {
        _size      = std::move( size );
        _type      = type;
        _precision = precision;
        _channels  = c;
        _isHDR     = hdr;
        _pixels    = std::make_shared<TexturePixels>( std::move( data ) );
        _format    = fmt;
        _sizeBytes = computeSizeInBytes();
        _gpuFormat = getRecommendedGPUFormat( _format, _precision, _channels );
    }

    AXION_FORCE_INLINE size_t computeSizeInBytes() const {
        if ( _isHDR )
            return std::get<std::vector<float>>( *_pixels ).size() * sizeof( float );
        return std::get<std::vector<unsigned char>>( *_pixels ).size() * sizeof( unsigned char );
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

    std::shared_ptr<TexturePixels> _pixels;
    size_t                         _sizeBytes = 0;
};

typedef Texture::SamplerDescription SamplerDesc;

} // namespace Core::Assets

AXION_NAMESPACE_END