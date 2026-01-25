#pragma once
#include <Axion/Core/Assets/Material.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class UnlitMaterial : public Material
{
    AXION_DECLARE_MATERIAL_ARCH_NAME( "Unlit" );

public:
    struct alignas( 16 ) GPUPayload {
        Math::Vec3 color;
        float      emissivePower;
        uint       textureIndex;
        float      overrideStrength;
        float      padding[2];
    };

    ~UnlitMaterial() override = default;

    void setEmissivePower( float power ) {
        if ( _emissivePower != power )
        {
            _emissivePower = power;
            markDirty();
        }
    }

    void setColor( const Math::Vec3& color ) {
        if ( _color != color )
        {
            _color = color;
            markDirty();
        }
    }
    void setColorTexture( TextureHandle handle ) {
        if ( _colorTextureHandle != handle )
        {
            _colorTextureHandle = handle;
            markDirty();
        }
    }
    void setTextureOverrideStrength( float strength ) {
        if ( _textureOverrideStrength != strength )
        {
            _textureOverrideStrength = strength;
            markDirty();
        }
    }

    [[nodiscard]] float             getEmissivePower() const { return _emissivePower; }
    [[nodiscard]] const Math::Vec3& getColor() const { return _color; }
    [[nodiscard]] TextureHandle     getColorTexture() const { return _colorTextureHandle; }
    [[nodiscard]] float             getTextureOverrideStrength() const { return _textureOverrideStrength; }

    uint getPayloadSize() const override {
        return sizeof( GPUPayload );
    };
    void writePayload( void* dest ) const override {

        GPUPayload tempPacket;
        tempPacket.color            = _color;
        tempPacket.emissivePower    = _emissivePower;
        tempPacket.overrideStrength = _textureOverrideStrength;

        // if ( _colorTextureHandle.isValid() && resolver )
        //     tempPacket.textureIndex = resolver( _colorTextureHandle );
        // else
        tempPacket.textureIndex = 0;

        tempPacket.padding[0] = 0.0f;
        tempPacket.padding[1] = 0.0f;

        std::memcpy( dest, &tempPacket, sizeof( GPUPayload ) );
    }

private:
    friend class AssetManager;

    explicit UnlitMaterial( std::string name )
        : Material( std::move( name ) ) {}

    float         _emissivePower           = 1.0f;
    Math::Vec3    _color                   = { 1.0f, 1.0f, 1.0f };
    float         _textureOverrideStrength = 1.0f;
    TextureHandle _colorTextureHandle;
};

} // namespace Core::Assets

AXION_NAMESPACE_END

AXION_REGISTER_MATERIAL( UnlitMaterial ) {

    desc.name = Axion::Core::Assets::UnlitMaterial::ARCHETYPE;

    desc.payloadSize = sizeof( Axion::Core::Assets::UnlitMaterial::GPUPayload );

    desc.topologiesSupported = Axion::Core::Render::MaterialTopologyTriangles;

    Axion::Core::Render::MaterialArchetypePassConfig pass;
    pass.passType    = Axion::Core::Render::MaterialPassType::Opaque;
    pass.shaderPath  = AXION_SHADER_DIR "/Slang/Materials/Unlit.slang";
    pass.entryPoints = {
        { "vsForward", Axion::Graphics::ShaderType::Vertex },
        { "psForward", Axion::Graphics::ShaderType::Pixel } };

    desc.passConfigs.push_back( pass );
}
