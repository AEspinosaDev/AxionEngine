#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Common/Containers/STLWrapper/Vector.h>
#include <Axion/Common/Containers/STLWrapper/String.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

struct MaterialArchetypeInfo {
    const char* name;
    const char* shaderModule;
    const char* shaderSpcecializationType;
};

class GlobalMaterialRegistry
{
public:
    static void registerMaterial( Axion::StringView name, MaterialArchetypeInfo info );
    static void enumerate( std::function<void( StringView name, MaterialArchetypeInfo info )> visitor );
};

/**
 * Macro used to easy define a Material Archetype.
 */
#define AXION_DECLARE_MATERIAL_ARCH( STRING_NAME, STRING_SHADER, STRING_SPECIALIZATION_TYPE )                           \
public:                                                                                                                 \
    static constexpr MaterialArchetypeInfo ARCHETYPE_INFO = { STRING_NAME, STRING_SHADER, STRING_SPECIALIZATION_TYPE }; \
                                                                                                                        \
    virtual MaterialArchetypeInfo getArchetypeInfo() const override { return ARCHETYPE_INFO; }

    
/**
 * Macro used to easy register a Material Archetype for the renderers.
 */
#define AXION_REGISTER_MATERIAL( CLASS_NAME )                                  \
    namespace {                                                                \
        struct Register##CLASS_NAME {                                          \
            Register##CLASS_NAME() {                                           \
                Axion::Core::Assets::GlobalMaterialRegistry::registerMaterial( \
                    CLASS_NAME::ARCHETYPE_INFO.name,                           \
                    CLASS_NAME::ARCHETYPE_INFO );                              \
            }                                                                  \
        };                                                                     \
        static Register##CLASS_NAME global_reg_##CLASS_NAME;                   \
    }

/**
 * @brief Bitmask flags to configure the mesh import process.
 * These flags control post-processing steps and additional resource loading.
 */
enum MeshImportFlags : u32
{
    MeshImportNone            = 1 << 0,
    MeshImportLoadMaterials   = 1 << 1,
    MeshImportLoadTextures    = 1 << 2,
    MeshImportLoadAnimations  = 1 << 3,
    MeshImportComputeTangents = 1 << 4,
    MeshImportComputeBounds   = 1 << 5,
    MeshImportAsLines         = 1 << 6,
    MeshImportAsPoints        = 1 << 7,
    MeshImportAsMeshlet       = 1 << 8,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( MeshImportFlags );

enum TextureImportFlags : u32
{
    TextureImportNone              = 1 << 0,
    TextureImportAsLinear          = 1 << 1,
    TextureImportAsGamma           = 1 << 2,
    TextureImportForce4Channels    = 1 << 3,
    TextureImportFlipVertically    = 1 << 4,
    TextureImportGenerateMipmaps   = 1 << 5,
    TextureImportAnisotropicFilter = 1 << 6,
    TextureImportAs3DTexture       = 1 << 7,
    TextureImportAsCubeMap         = 1 << 8,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( TextureImportFlags );

enum class TextureType : byte
{
    Texture2D,
    CubeMap,
    Texture3D,
};

enum class TextureFormat : byte
{
    Gamma,  // Albedo/Diffuse (sRGB)
    Linear, // Normal Map, Roughness, Metallic (Linear)
    HDR     // Skyboxes, Lightmaps (Float)
};

enum class TexturePrecision : byte
{
    U8,  // 8-bit unsigned (Standard)
    F16, // 16-bit float (Half HDR)
    F32  // 32-bit float (Full HDR)
};

constexpr Graphics::Format getRecommendedGPUFormat( const TextureFormat    fmt,
                                                    const TexturePrecision prec,
                                                    const u32              channels ) {

    // --- FLOAT 32 (Full HDR) ---
    if ( prec == TexturePrecision::F32 )
    {
        switch ( channels )
        {
            case 1:
                return Graphics::Format::R32_FLOAT;
            case 2:
                return Graphics::Format::RG32_FLOAT;
            case 3:
                return Graphics::Format::RGB32_FLOAT;
            case 4:
                return Graphics::Format::RGBA32_FLOAT;
            default:
                return Graphics::Format::UNKNOWN;
        }
    }

    // --- FLOAT 16 (Half HDR) ---
    if ( prec == TexturePrecision::F16 )
    {
        switch ( channels )
        {
            case 1:
                return Graphics::Format::R16_FLOAT;
            case 2:
                return Graphics::Format::RG16_FLOAT;
            case 3:
                return Graphics::Format::RGBA16_FLOAT;
            case 4:
                return Graphics::Format::RGBA16_FLOAT;
            default:
                return Graphics::Format::UNKNOWN;
        }
    }

    // --- UINT 8 (Standard) ---
    if ( prec == TexturePrecision::U8 )
    {
        if ( fmt == TextureFormat::Gamma )
        {
            switch ( channels )
            {
                case 1:
                    return Graphics::Format::R8_UNORM;
                case 2:
                    return Graphics::Format::RG8_UNORM;
                // RGB8 NOT SUPPORTED DX12/Vulkan (alignment).
                case 3:
                    return Graphics::Format::SRGBA8_UNORM;
                case 4:
                    return Graphics::Format::SRGBA8_UNORM;
                default:
                    return Graphics::Format::UNKNOWN;
            }
        } else // TextureFormat::Linear
        {
            switch ( channels )
            {
                case 1:
                    return Graphics::Format::R8_UNORM;
                case 2:
                    return Graphics::Format::RG8_UNORM;
                case 3:
                    return Graphics::Format::RGBA8_UNORM; // Padding
                case 4:
                    return Graphics::Format::RGBA8_UNORM;
                default:
                    return Graphics::Format::UNKNOWN;
            }
        }
    }

    return Graphics::Format::UNKNOWN;
}

} // namespace Core::Assets

AXION_NAMESPACE_END