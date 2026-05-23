#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Common/Containers/String.h"
#include "Axion/Common/Math.h"

AXION_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////
// Graphics Related General Definitions
////////////////////////////////////////////////////////////////////////

namespace Graphics {

enum class API : u32
{
    DirectX12,
    Vulkan,
    Invalid
};
enum class PlatformType : u32
{
    Win32,
    GLFW,
    SDL,
    Invalid
};

enum class PresentMode : u32
{
    Immediate,
    Vsync
};

enum class BufferingType : u32
{
    Mono   = 1,
    Double = 2,
    Triple = 3,
};

enum class Format : byte
{
    UNKNOWN,

    R8_UINT,
    R8_SINT,
    R8_UNORM,
    R8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    R16_UINT,
    R16_SINT,
    R16_UNORM,
    R16_SNORM,
    R16_FLOAT,
    BGRA4_UNORM,
    B5G6R5_UNORM,
    B5G5R5A1_UNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    BGRA8_UNORM,
    BGRX8_UNORM,
    SRGBA8_UNORM,
    SBGRA8_UNORM,
    SBGRX8_UNORM,
    R10G10B10A2_UNORM,
    R11G11B10_FLOAT,
    RG16_UINT,
    RG16_SINT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_FLOAT,
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    D16,
    D24S8,
    X24G8_UINT,
    D32,
    D32S8,
    X32G8_UINT,

    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    COUNT,
};

constexpr size_t getFormatBytes( Format format ) {
    switch ( format )
    {
        // 8-bit
        case Format::R8_UINT:
        case Format::R8_SINT:
        case Format::R8_UNORM:
        case Format::R8_SNORM:
            return 1;

        // RG8
        case Format::RG8_UINT:
        case Format::RG8_SINT:
        case Format::RG8_UNORM:
        case Format::RG8_SNORM:
            return 2;

        // 16-bit
        case Format::R16_UINT:
        case Format::R16_SINT:
        case Format::R16_UNORM:
        case Format::R16_SNORM:
        case Format::R16_FLOAT:
            return 2;

        // 16-bit packed
        case Format::BGRA4_UNORM:
        case Format::B5G6R5_UNORM:
        case Format::B5G5R5A1_UNORM:
            return 2;

        // RGBA8 / 8-bit per channel
        case Format::RGBA8_UINT:
        case Format::RGBA8_SINT:
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SNORM:
        case Format::BGRA8_UNORM:
        case Format::BGRX8_UNORM:
        case Format::SRGBA8_UNORM:
        case Format::SBGRA8_UNORM:
        case Format::SBGRX8_UNORM:
            return 4;

        // HDR-ish
        case Format::R10G10B10A2_UNORM:
        case Format::R11G11B10_FLOAT:
            return 4;

        // RG16
        case Format::RG16_UINT:
        case Format::RG16_SINT:
        case Format::RG16_UNORM:
        case Format::RG16_SNORM:
        case Format::RG16_FLOAT:
            return 4;

        // 32-bit single
        case Format::R32_UINT:
        case Format::R32_SINT:
        case Format::R32_FLOAT:
            return 4;

        // RGBA16
        case Format::RGBA16_UINT:
        case Format::RGBA16_SINT:
        case Format::RGBA16_FLOAT:
        case Format::RGBA16_UNORM:
        case Format::RGBA16_SNORM:
            return 8;

        // RG32
        case Format::RG32_UINT:
        case Format::RG32_SINT:
        case Format::RG32_FLOAT:
            return 8;

        // RGB32
        case Format::RGB32_UINT:
        case Format::RGB32_SINT:
        case Format::RGB32_FLOAT:
            return 12;

        // RGBA32
        case Format::RGBA32_UINT:
        case Format::RGBA32_SINT:
        case Format::RGBA32_FLOAT:
            return 16;

        // Depth / stencil
        case Format::D16:
            return 2;
        case Format::D24S8:
            return 4;
        case Format::D32:
            return 4;
        case Format::D32S8:
            return 8;
        case Format::X24G8_UINT:
            return 4;
        case Format::X32G8_UINT:
            return 8;

        // BC compressed (block-compressed: 4x4 blocks, size in bytes per block)
        case Format::BC1_UNORM:
        case Format::BC1_UNORM_SRGB:
        case Format::BC4_UNORM:
        case Format::BC4_SNORM:
            return 8; // bytes per 4x4 block
        case Format::BC2_UNORM:
        case Format::BC2_UNORM_SRGB:
        case Format::BC3_UNORM:
        case Format::BC3_UNORM_SRGB:
        case Format::BC5_UNORM:
        case Format::BC5_SNORM:
        case Format::BC6H_UFLOAT:
        case Format::BC6H_SFLOAT:
        case Format::BC7_UNORM:
        case Format::BC7_UNORM_SRGB:
            return 16; // bytes per 4x4 block

        default:
            return 0;
    }
}

enum class TextureDimension : byte
{
    Unknown,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    TextureCube,
    TextureCubeArray,
    Texture2DMS,
    Texture2DMSArray,
    Texture3D

};

enum class Filter
{
    Nearest,
    Linear
};
enum class AddressMode
{
    Repeat,
    Wrap,
    Clamp,
    Mirror,
    Border
};

struct ClearValue {
    Math::Vec4 color   = { 0.0, 0.0, 0.0, 1.0 }; // RGBA for RTV/UAV
    float      depth   = 1.0f;                   // depth for DSV
    byte       stencil = 0;                      // stencil for DSV

    inline bool operator==( const ClearValue o ) const {
        return color == o.color && depth == o.depth && stencil == o.stencil;
    }
    inline bool operator!=( const ClearValue o ) const {
        return !operator==( o );
    }
};

enum class PrimitiveTopology : byte
{
    Undefined = 0,
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    PatchList,
    Count
};

AXION_ENUM_CLASS_FLAG_OPERATORS( PrimitiveTopology )

String32 getTopologyString( PrimitiveTopology type ) {
    switch ( type )
    {
        case PrimitiveTopology::TriangleList:
            return "TriList";
        case PrimitiveTopology::TriangleStrip:
            return "TriStrip";
        case PrimitiveTopology::TriangleFan:
            return "TriFan";
        case PrimitiveTopology::LineList:
            return "LineList";
        case PrimitiveTopology::LineStrip:
            return "LineStrip";
        case PrimitiveTopology::PointList:
            return "PntList";

        default:
            return "Unknown";
    }
}

enum class FillMode : byte
{
    Solid     = 0,
    Wireframe = 1
};

enum class CullMode : byte
{
    None  = 0,
    Front = 1,
    Back  = 2
};

enum class CompareOp : byte
{
    Never        = 0,
    Less         = 1,
    Equal        = 2,
    LessEqual    = 3,
    Greater      = 4,
    NotEqual     = 5,
    GreaterEqual = 6,
    Always       = 7
};

enum class BlendFactor : byte
{
    Zero = 0,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    SrcAlphaSaturate
};

enum class BlendOp : byte
{
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

// View type flags (bitmask)
enum TextureViewFlags : u32
{
    TextureViewNone            = 0,
    TextureViewShaderResource  = 1 << 0, // SRV in DX12, VK_IMAGE_VIEW_TYPE_*
    TextureViewRenderTarget    = 1 << 1, // RTV / VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    TextureViewDepthStencil    = 1 << 2, // DSV / VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    TextureViewUnorderedAccess = 1 << 3  // UAV / VK_IMAGE_USAGE_STORAGE_BIT
};

AXION_ENUM_CLASS_FLAG_OPERATORS( TextureViewFlags )

enum BufferViewFlags : u32
{
    BufferViewNone            = 0,
    BufferViewShaderResource  = 1 << 0, // SRV / VK_DESCRIPTOR_TYPE_STORAGE_BUFFER/UNIFORM_TEXEL
    BufferViewUnorderedAccess = 1 << 1, // UAV / VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    BufferViewConstantBuffer  = 1 << 2  // CBV / VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
};

AXION_ENUM_CLASS_FLAG_OPERATORS( BufferViewFlags )

enum class MemoryUsage : u32
{
    Unknown    = 0,
    CPUVisible = 1 << 0, // Upload / staging
    GPUOnly    = 1 << 1,
    Readback   = 1 << 2
};
AXION_ENUM_CLASS_FLAG_OPERATORS( MemoryUsage )

enum class BufferUsage : u32
{
    None                  = 0,
    Vertex                = 1 << 0,
    Index                 = 1 << 1,
    Uniform               = 1 << 2, // CB / UBO
    Storage               = 1 << 3, // UAV / SSBO
    Indirect              = 1 << 4,
    TransferSrc           = 1 << 5,
    TransferDst           = 1 << 6,
    AccelerationStructure = 1 << 7,
};
AXION_ENUM_CLASS_FLAG_OPERATORS( BufferUsage )

/// @brief Garbage Collection TTL presets for transient resources (in frames).
enum class GCMode
{
    LowMemory  = 60,  ///< Aggressive cleanup (1s at 60fps).
    AvgMemory  = 180, ///< Balanced (3s).
    HighMemory = 360  ///< Relaxed cleanup (5s), reduces allocation spikes.
};

enum class ShaderType : u16
{
    None = 0x0000,

    Compute = 0x0020,

    Vertex        = 0x0001,
    Hull          = 0x0002,
    Domain        = 0x0004,
    Geometry      = 0x0008,
    Pixel         = 0x0010,
    Amplification = 0x0040,
    Mesh          = 0x0080,
    AllGraphics   = 0x00DF,

    RayGeneration = 0x0100,
    AnyHit        = 0x0200,
    ClosestHit    = 0x0400,
    Miss          = 0x0800,
    Intersection  = 0x1000,
    Callable      = 0x2000,
    AllRayTracing = 0x3F00,

    All = 0x3FFF,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( ShaderType )

namespace Shader {

struct EntryPoint {
    String32   name; // Eg: "vsMain"
    ShaderType type;
};

struct PreprocessorDefine {
    String64 name;
    String32 value;
};

} // namespace Shader

struct RenderState {
    Graphics::PrimitiveTopology topology   = Graphics::PrimitiveTopology::TriangleList;
    Graphics::FillMode          fillMode   = Graphics::FillMode::Solid;
    Graphics::CullMode          cullMode   = Graphics::CullMode::Front;
    Graphics::BlendOp           blendOp    = Graphics::BlendOp::Add;
    Graphics::CompareOp         depthOp    = Graphics::CompareOp::LessEqual;
    bool                        depthWrite = true;
    bool                        depthTest  = true;
    // To be extended later ...

    bool operator==( const RenderState& o ) const {
        return topology == o.topology &&
               fillMode == o.fillMode &&
               cullMode == o.cullMode &&
               blendOp == o.blendOp &&
               depthOp == o.depthOp &&
               depthWrite == o.depthWrite &&
               depthTest == o.depthTest;
    };
    bool operator!=( const RenderState& o ) const {
        return !operator==( o );
    };
};

} // namespace Graphics
AXION_NAMESPACE_END
