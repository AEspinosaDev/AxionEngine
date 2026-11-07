#pragma once
#include <Axion/Core/Assets/Material.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

/// @brief Classic Blinn-Phong material: lambert diffuse plus a specular lobe whose glossiness is the shininess exponent.
class PhongMaterial : public Material
{
    AXION_DECLARE_MATERIAL_ARCH(
        "Phong",
        "Phong.slang",
        "PhongMaterial" );

public:
    /// @brief 32 bytes. Must match PhongPayload in Phong.slang
    struct alignas( 16 ) GPUPayload {
        // Chunk 0
        Math::Vec3 diffuse;
        float      shininess;

        // Chunk 1
        Math::Vec3 specular;
        u32        diffuseTexIndex;
    };

    ~PhongMaterial() override = default;

    void setDiffuse( const Math::Vec3& color ) {
        if ( _diffuse != color )
        {
            _diffuse = color;
            markDirty();
        }
    }

    void setSpecular( const Math::Vec3& color ) {
        if ( _specular != color )
        {
            _specular = color;
            markDirty();
        }
    }

    /// @brief Glossiness: exponent of the specular lobe. Higher means a smaller and sharper highlight
    void setShininess( float shininess ) {
        if ( _shininess != shininess )
        {
            _shininess = shininess;
            markDirty();
        }
    }

    /// @brief Multiplies the diffuse color
    void setDiffuseTexture( TextureHandle handle ) {
        if ( _diffuseMapHandle != handle )
        {
            _diffuseMapHandle = handle;
            markDirty();
        }
    }

    [[nodiscard]] const Math::Vec3& getDiffuse() const { return _diffuse; }
    [[nodiscard]] const Math::Vec3& getSpecular() const { return _specular; }
    [[nodiscard]] float             getShininess() const { return _shininess; }
    [[nodiscard]] TextureHandle     getDiffuseTexture() const { return _diffuseMapHandle; }

    u32 getPayloadSize() const override {
        return sizeof( GPUPayload );
    };

    void writePayload( void* dest, const TextureResolver& resolver ) const override {
        GPUPayload p;

        p.diffuse         = _diffuse;
        p.shininess       = _shininess;
        p.specular        = _specular;
        p.diffuseTexIndex = resolver ? resolver( _diffuseMapHandle ) : AXION_INVALID_U32;

        std::memcpy( dest, &p, sizeof( GPUPayload ) );
    }

    STLW::Vector<TextureHandle> getTextureHandles() const override { return { _diffuseMapHandle }; };

private:
    friend class AssetManager;

    explicit PhongMaterial( StringView name )
        : Material( name ) {}

    Math::Vec3    _diffuse   = { 0.5f, 0.5f, 0.5f };
    Math::Vec3    _specular  = { 0.5f, 0.5f, 0.5f };
    float         _shininess = 32.0f;
    TextureHandle _diffuseMapHandle;
};

AXION_REGISTER_MATERIAL( PhongMaterial );

} // namespace Core::Assets

AXION_NAMESPACE_END
