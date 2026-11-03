#pragma once
#include <Axion/Core/Assets/Material.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class DebugMaterial : public Material
{
     AXION_DECLARE_MATERIAL_ARCH(
        "Debug",
        "Debug.slang",
        "DebugMaterial" );

public:
    struct alignas( 16 ) GPUPayload {
        Math::Vec4 color;
    };

    ~DebugMaterial() override = default;

    u32 getPayloadSize() const override {
        return sizeof( GPUPayload );
    };
    void writePayload( void* dest, const TextureResolver& resolver ) const override {

        GPUPayload tempPacket;
        tempPacket.color = { 1.0, 0.0, 0.5, 1.0 };

        std::memcpy( dest, &tempPacket, sizeof( GPUPayload ) );
    }

    STLW::Vector<TextureHandle> getTextureHandles() const override { return {}; };

private:
    friend class AssetManager;

    explicit DebugMaterial( StringView name )
        : Material(  name  ) {}
};

// AXION_REGISTER_MATERIAL( DebugMaterial );
} // namespace Core::Assets

AXION_NAMESPACE_END

