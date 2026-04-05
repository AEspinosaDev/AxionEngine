
#pragma once
#include <Axion/Common/Containers/STLWrapper/String.h>
#include <Axion/Common/Containers/STLWrapper/Vector.h>
#include <Axion/Common/Graphics/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Renderer feature flags
enum RendererFlags : u32
{
    RendererNone             = 0,
    RendererEnableGUI        = 1 << 0,
    RendererEnableDebug      = 1 << 1,
    RendererEnableFXAA       = 1 << 2,
    RendererEnableMSAAx2     = 1 << 3,
    RendererEnableMSAAx4     = 1 << 4,
    RendererEnableMSAAx8     = 1 << 5,
    RendererEnableTAA        = 1 << 6,
    RendererEnableHDR        = 1 << 7,
    RendererEnableDLSS       = 1 << 8,
    RendererEnableGPUCulling = 1 << 9,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( RendererFlags )

enum class TopologyType : byte
{
    Triangles = 0,
    Lines     = 1,
    Points    = 2,
    Meshlets  = 3,
    Count
};

static constexpr Graphics::PrimitiveTopology toGFXTopology( TopologyType type ) {
    switch ( type )
    {
        case TopologyType::Triangles:
            return Graphics::PrimitiveTopology::TriangleList;
        case TopologyType::Lines:
            return Graphics::PrimitiveTopology::LineList;
        case TopologyType::Points:
            return Graphics::PrimitiveTopology::PointList;
        case TopologyType::Meshlets:
            return Graphics::PrimitiveTopology::TriangleList;
        default:
            return Graphics::PrimitiveTopology::TriangleList;
    }
}

enum class MaterialPassType : byte
{
    Opaque      = 0,
    Blend       = 1,
    Geometry    = 2,
    Composition = 3,
    Depth       = 4,
    Shadow      = 5,
    Raytracing  = 6,
    Wireframe   = 7,

    Count
};

enum MaterialTopologyFlags : byte
{
    MaterialTopologyNone      = 0,
    MaterialTopologyTriangles = 1 << 0,
    MaterialTopologyLines     = 1 << 1,
    MaterialTopologyPoints    = 1 << 2,
    MaterialTopologyMeshlets  = 1 << 3,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( MaterialTopologyFlags )

struct MaterialArchetypePassConfig {
    MaterialPassType                           passType;
    STLW::String                               shaderPath;
    STLW::Vector<Graphics::Shader::EntryPoint> entryPoints;
    STLW::String                               customIncludePath;

    Graphics::FillMode  fillMode   = Graphics::FillMode::Solid;
    Graphics::CullMode  cullMode   = Graphics::CullMode::Front;
    Graphics::BlendOp   blendOp    = Graphics::BlendOp::Add;
    Graphics::CompareOp depthOp    = Graphics::CompareOp::LessEqual;
    bool                depthWrite = true;
    bool                depthTest  = true;

    String64 customPassAlias = "";
};

struct MaterialArchetypeDesc {
    String64                                  name;
    STLW::Vector<MaterialArchetypePassConfig> passConfigs;
    MaterialTopologyFlags                     topologiesSupported = MaterialTopologyTriangles;
    u32                                       payloadSize         = 0;
};

enum class MemoryBudgetPreset : byte
{
    Balanced,    // Balanced memory distribution for typical use cases (Action/RTS games might need a different ratio)
    AssetHeavy,  // More memory for assets, less for per-frame dynamic data (Good for open-world games with lots of assets)
    DynamicHeavy // Less memory for assets, more for per-frame dynamic data (Good for action games with lots of dynamic effects and streaming)
};

/**
 * MemoryBudget struct defines configurable memory limits for different categories of resource allocations in the rendering engine.
 * It is designed to help manage and optimize memory usage based on the application's needs and the underlying
 */
struct MemoryBudget {
    struct Device {
        u64 maxTextureAlloc      = GIGABYTES( 2ull ); ///< Logical cap for loaded textures before forcing mipmap eviction.
        u64 maxGeometryAlloc     = MBYTES( 512 );     ///< Memory reservation persistent static geometry buffer -Vertex/Index- (512MB default)
        u64 maxMaterialAlloc     = MBYTES( 16 );      ///< Memory reservation persistent static material buffer (16MB default)
        u64 maxRenderTargetAlloc = MBYTES( 512 );     ///< Logical cap for G-Buffers, Depth Stencil, and Shadow Maps.
        u64 maxRaytracingAlloc   = MBYTES( 256 );     ///< Memory reservation for BVH structures (TLAS/BLAS) if applicable.

        u32 maxMtlTextures = 8192;
        u32 maxMtlSamplers = 128;
    };
    struct Host {
        u64 maxPersistentAlloc        = MBYTES( 256 ); ///< Memory reservation for persistent data allocations.
        u64 maxTransientAllocPerFrame = MBYTES( 128 ); ///< Memory reservation for transient data allocations per frame.
    };
    struct Shared {
        u64 maxConstantAllocPerFrame = MBYTES( 64 );   ///< Memory reservation for per-frame volatile buffer
        u64 maxUploadAllocPerFrame   = MBYTES( 128 );  ///< Memory reservation for per-frame transient upload buffer
        u64 maxExecutableAlloc       = KBYTES( 1024 ); ///< Memory reservation for per-frame Shader Binding Tables and Indirect Commands data.
    };

    Device device {};
    Host   host {};
    Shared shared {};

    static MemoryBudget configureBudget( u64                     totalRamBudget,
                                         u64                     totalVramBudget,
                                         MemoryBudgetPreset      preset        = MemoryBudgetPreset::Balanced,
                                         Graphics::BufferingType bufferingType = Graphics::BufferingType::Double ) {
        MemoryBudget budget;

        budget.shared.maxConstantAllocPerFrame = MBYTES( 16 );
        budget.shared.maxUploadAllocPerFrame   = ( preset == MemoryBudgetPreset::AssetHeavy ) ? MBYTES( 256 ) : MBYTES( 128 );

        budget.device.maxMaterialAlloc   = MBYTES( 16 );
        budget.shared.maxExecutableAlloc = KBYTES( 1024 );

        u64 availableVram = totalVramBudget - ( budget.device.maxMaterialAlloc + budget.shared.maxExecutableAlloc );

        switch ( preset )
        {
            case MemoryBudgetPreset::Balanced:
                // RAM Distribution
                budget.host.maxPersistentAlloc        = ( totalRamBudget * 80 ) / 100;
                budget.host.maxTransientAllocPerFrame = ( ( totalRamBudget * 20 ) / 100 ) / static_cast<u32>( bufferingType );

                // VRAM Distribution
                budget.device.maxGeometryAlloc     = ( availableVram * 25 ) / 100; // 25% Geometry
                budget.device.maxRenderTargetAlloc = ( availableVram * 25 ) / 100; // 25% Render Targets
                budget.device.maxRaytracingAlloc   = ( availableVram * 15 ) / 100; // 15% Raytracing BVH
                budget.device.maxTextureAlloc      = ( availableVram * 35 ) / 100; // 35% Textures
                break;

            case MemoryBudgetPreset::AssetHeavy:
                // RAM Distribution
                budget.host.maxPersistentAlloc        = ( totalRamBudget * 90 ) / 100;
                budget.host.maxTransientAllocPerFrame = ( ( totalRamBudget * 10 ) / 100 ) / static_cast<u32>( bufferingType );

                // VRAM Distribution
                budget.device.maxGeometryAlloc     = ( availableVram * 20 ) / 100; // 20% Geometry
                budget.device.maxRenderTargetAlloc = ( availableVram * 20 ) / 100; // 20% Render Targets
                budget.device.maxRaytracingAlloc   = ( availableVram * 10 ) / 100; // 10% Raytracing BVH
                budget.device.maxTextureAlloc      = ( availableVram * 50 ) / 100; // 50% Textures
                break;

            case MemoryBudgetPreset::DynamicHeavy:
                // RAM Distribution
                budget.host.maxPersistentAlloc        = ( totalRamBudget * 70 ) / 100;
                budget.host.maxTransientAllocPerFrame = ( ( totalRamBudget * 30 ) / 100 ) / static_cast<u32>( bufferingType );

                // VRAM Distribution (Assuming more dynamic G-Buffers, multiple shadow cascades, fewer static textures)
                budget.device.maxGeometryAlloc     = ( availableVram * 10 ) / 100; // 10% Geometry
                budget.device.maxRenderTargetAlloc = ( availableVram * 35 ) / 100; // 35% Render Targets
                budget.device.maxRaytracingAlloc   = ( availableVram * 5 ) / 100;  //  5% Raytracing BVH
                budget.device.maxTextureAlloc      = ( availableVram * 50 ) / 100; // 50% Textures
                break;
        }

        return budget;
    }
};


} // namespace Core::Render

AXION_NAMESPACE_END