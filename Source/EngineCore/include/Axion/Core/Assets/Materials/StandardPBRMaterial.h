#pragma once
#include <Axion/Core/Assets/Material.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class StandardPBRMaterial : public Material
{
    AXION_DECLARE_MATERIAL_ARCH_NAME( "StandardPBR" );

public:
    // Total: 64 bytes
    struct alignas( 16 ) GPUPayload {
        // Chunk 0
        Math::Vec3 albedo;
        float      roughness;

        // Chunk 1
        Math::Vec3 emissive;
        float      metallic;

        // Chunk 2
        float normalScale;
        float aoStrength;
        uint  albedoTexIndex;
        uint  normalTexIndex;

        // Chunk 3
        uint  armTexIndex;      // Packed (AO, Roughness, Metallic)
        uint  emissiveTexIndex; // Texture ID
        float padding[2];
    };

    ~StandardPBRMaterial() override = default;

    void setAlbedo( const Math::Vec3& color ) {
        if ( _albedo != color )
        {
            _albedo = color;
            markDirty();
        }
    }

    void setRoughness( float roughness ) {
        if ( _roughness != roughness )
        {
            _roughness = roughness;
            markDirty();
        }
    }

    void setMetallic( float metallic ) {
        if ( _metallic != metallic )
        {
            _metallic = metallic;
            markDirty();
        }
    }

    void setEmissive( const Math::Vec3& color, float intensity ) {
        Math::Vec3 finalEmissive = color * intensity;
        if ( _emissive != finalEmissive )
        {
            _emissive = finalEmissive;
            markDirty();
        }
    }

    void setNormalScale( float scale ) {
        if ( _normalScale != scale )
        {
            _normalScale = scale;
            markDirty();
        }
    }

    void setAlbedoTexture( TextureHandle handle ) {
        if ( _albedoMapHandle != handle )
        {
            _albedoMapHandle = handle;
            markDirty();
        }
    }

    void setNormalTexture( TextureHandle handle ) {
        if ( _normalMapHandle!= handle )
        {
            _normalMapHandle = handle;
            markDirty();
        }
    }

    // ARM = Ambient Occlusion (R), Roughness (G), Metallic (B)
    void setARMTexture( TextureHandle handle ) {
        if ( _armMapHandle != handle )
        {
            _armMapHandle = handle;
            markDirty();
        }
    }

    [[nodiscard]] const Math::Vec3& getAlbedo() const { return _albedo; }
    [[nodiscard]] float             getRoughness() const { return _roughness; }
    [[nodiscard]] float             getMetallic() const { return _metallic; }
    [[nodiscard]] const Math::Vec3& getEmissive() const { return _emissive; }

    uint getPayloadSize() const override {
        return sizeof( GPUPayload );
    };

    void writePayload( void* dest, const TextureResolver& resolver ) const override {
        GPUPayload p;

        p.albedo    = _albedo;
        p.roughness = _roughness;
        p.emissive  = _emissive;
        p.metallic  = _metallic;

        p.normalScale = _normalScale;
        p.aoStrength  = 1.0f;

        if ( resolver )
        {

            p.albedoTexIndex   = resolver( _albedoMapHandle );
            p.normalTexIndex   = resolver( _normalMapHandle );
            p.armTexIndex      = resolver( _armMapHandle );
            p.emissiveTexIndex = resolver( _emissiveMapHandle );
        }

        p.padding[0] = 0.0f;
        p.padding[1] = 0.0f;

        std::memcpy( dest, &p, sizeof( GPUPayload ) );
    }

    std::vector<TextureHandle> getTextureHandles() const override { return { _albedoMapHandle, _normalMapHandle, _armMapHandle, _emissiveMapHandle}; };

private:
    friend class AssetManager;

    explicit StandardPBRMaterial( std::string name )
        : Material( std::move( name ) ) {}

    Math::Vec3 _albedo      = { 0.5f, 0.5f, 0.5f };
    Math::Vec3 _emissive    = { 0.0f, 0.0f, 0.0f };
    float      _roughness   = 0.5f;
    float      _metallic    = 0.0f;
    float      _normalScale = 1.0f;

    TextureHandle _albedoMapHandle;
    TextureHandle _normalMapHandle;
    TextureHandle _armMapHandle; // Packed: AO, Roughness, Metal
    TextureHandle _emissiveMapHandle;
};

} // namespace Core::Assets

AXION_NAMESPACE_END

AXION_REGISTER_MATERIAL( StandardPBRMaterial ) {

    desc.name = Axion::Core::Assets::StandardPBRMaterial::ARCHETYPE;

    desc.payloadSize = sizeof( Axion::Core::Assets::StandardPBRMaterial::GPUPayload );

    desc.topologiesSupported = Axion::Core::Render::MaterialTopologyTriangles;

    Axion::Core::Render::MaterialArchetypePassConfig pass;
    pass.passType   = Axion::Core::Render::MaterialPassType::Opaque;
    pass.shaderPath = AXION_SHADER_DIR "/Slang/Materials/StandardPBR.slang";

    pass.entryPoints = {
        { "vsForward", Axion::Graphics::ShaderType::Vertex },
        { "psForward", Axion::Graphics::ShaderType::Pixel } };
    pass.customIncludePath = AXION_SHADER_DIR "/Slang/BxDFs";

    desc.passConfigs.push_back( pass );
}