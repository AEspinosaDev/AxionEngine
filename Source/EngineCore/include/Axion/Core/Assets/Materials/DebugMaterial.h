#pragma once
#include <Axion/Core/Assets/Material.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class DebugMaterial : public Material
{
    AXION_DECLARE_MATERIAL_ARCH_NAME( "Debug" );

public:
    struct alignas( 16 ) GPUPayload {
        Math::Vec4 color;
    };

    ~DebugMaterial() override = default;

    uint getPayloadSize() const override {
        return sizeof( GPUPayload );
    };
    void writePayload( void* dest ) const override {

        GPUPayload tempPacket;
        tempPacket.color = { 1.0, 0.0, 0.5, 1.0 };

        std::memcpy( dest, &tempPacket, sizeof( GPUPayload ) );
    }

private:
    friend class AssetManager;

    explicit DebugMaterial( std::string name )
        : Material( std::move( name ) ) {}
};

} // namespace Core::Assets

AXION_NAMESPACE_END

AXION_REGISTER_MATERIAL( DebugMaterial ) {

    desc.name = Axion::Core::Assets::DebugMaterial::ARCHETYPE;

    desc.payloadSize = sizeof( Axion::Core::Assets::DebugMaterial::GPUPayload );

    desc.topologiesSupported = Axion::Core::Render::MaterialTopologyTriangles;

    Axion::Core::Render::MaterialArchetypePassConfig pass;
    pass.passType    = Axion::Core::Render::MaterialPassType::Opaque;
    pass.shaderPath  = AXION_SHADER_DIR "/Slang/Materials/Debug.slang";
    pass.entryPoints = {
        { "vsForward", Axion::Graphics::ShaderType::Vertex },
        { "psForward", Axion::Graphics::ShaderType::Pixel } };

    pass.depthWrite = false;
    pass.depthOp    = Axion::Graphics::CompareOp::Equal;

    desc.passConfigs.push_back( pass );
}
