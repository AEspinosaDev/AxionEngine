
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

struct MaterialArchetypeDesc {
    String64                       name;
    STLW::String                   archetypeShaderPath;
    Graphics::TopologySupportFlags topologiesSupported = Graphics::TopologySupportTriangleStrip;
    u32                            payloadSize         = 0;
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
    bool   strictVRAM = false; ///< If memory request surpass the limits on the GPU, strict mode doesnt let device allocate new memory

    enum class Preset : byte
    {
        Balanced,    // Balanced memory distribution for typical use cases (Action/RTS games might need a different ratio)
        AssetHeavy,  // More memory for assets, less for per-frame dynamic data (Good for open-world games with lots of assets)
        DynamicHeavy // Less memory for assets, more for per-frame dynamic data (Good for action games with lots of dynamic effects and streaming)
    };

    static MemoryBudget configureBudget( u64                     totalRamBudget,
                                         u64                     totalVramBudget,
                                         Preset                  preset        = Preset::Balanced,
                                         Graphics::BufferingType bufferingType = Graphics::BufferingType::Double ) {
        MemoryBudget budget;

        budget.shared.maxConstantAllocPerFrame = MBYTES( 16 );
        budget.shared.maxUploadAllocPerFrame   = ( preset == Preset::AssetHeavy ) ? MBYTES( 256 ) : MBYTES( 128 );

        budget.device.maxMaterialAlloc   = MBYTES( 16 );
        budget.shared.maxExecutableAlloc = KBYTES( 1024 );

        u64 availableVram = totalVramBudget - ( budget.device.maxMaterialAlloc + budget.shared.maxExecutableAlloc );

        switch ( preset )
        {
            case Preset::Balanced:
                // RAM Distribution
                budget.host.maxPersistentAlloc        = ( totalRamBudget * 80 ) / 100;
                budget.host.maxTransientAllocPerFrame = ( ( totalRamBudget * 20 ) / 100 ) / static_cast<u32>( bufferingType );

                // VRAM Distribution
                budget.device.maxGeometryAlloc     = ( availableVram * 25 ) / 100; // 25% Geometry
                budget.device.maxRenderTargetAlloc = ( availableVram * 25 ) / 100; // 25% Render Targets
                budget.device.maxRaytracingAlloc   = ( availableVram * 15 ) / 100; // 15% Raytracing BVH
                budget.device.maxTextureAlloc      = ( availableVram * 35 ) / 100; // 35% Textures
                break;

            case Preset::AssetHeavy:
                // RAM Distribution
                budget.host.maxPersistentAlloc        = ( totalRamBudget * 90 ) / 100;
                budget.host.maxTransientAllocPerFrame = ( ( totalRamBudget * 10 ) / 100 ) / static_cast<u32>( bufferingType );

                // VRAM Distribution
                budget.device.maxGeometryAlloc     = ( availableVram * 20 ) / 100; // 20% Geometry
                budget.device.maxRenderTargetAlloc = ( availableVram * 20 ) / 100; // 20% Render Targets
                budget.device.maxRaytracingAlloc   = ( availableVram * 10 ) / 100; // 10% Raytracing BVH
                budget.device.maxTextureAlloc      = ( availableVram * 50 ) / 100; // 50% Textures
                break;

            case Preset::DynamicHeavy:
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