#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Core/Render/IRasterizer.h>
#include <Axion/Graphics/ShaderCommon.h>

#include <Render/MaterialLibrary.h>
#include <Render/PassManager.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer {
namespace Config {

//-----------------------------------------------------------------------------
// Rasterizer Material Passes
//-----------------------------------------------------------------------------
enum class MaterialPassType : u32
{
    Visibility             = 0,
    VisibilityParticle     = 1,
    VisibilityResolve      = 2, ///< Binned: one indirect dispatch per resolve bin
    VisibilityResolveNaive = 3, ///< One full screen dispatch per material bundle (RendererUseNaiveResolve)
    // Shadow             = 4,
    // ForwardTransparent = 5,

    Count
};

//-----------------------------------------------------------------------------
// Rasterizer Layout Sets
//-----------------------------------------------------------------------------
enum class LayoutSetType : u32
{
    FrameBound = 0,
    PassBound  = 1,

    Count,
    PushConstant = Count
};
AXION_STATIC_ASSERT( (u32)LayoutSetType::PassBound == Graphics::PassIO::SET_INDEX, "Pass I/O set must match Graphics::PassIO (DECLARE_* macros)" );

//-----------------------------------------------------------------------------
// Rasterizer Layout Register Map 
//                                  Must match GlobalLayouts.slang !!
//-----------------------------------------------------------------------------

// Persistent buffers (geometry pages, materials), textures & samplers are bindless: heap indexed, no bindings

// Space 0: Frame data
constexpr u32 REG_FRAME_CBV_BASE = 0; // b0: gFrame
constexpr u32 REG_FRAME_SRV_BASE = 0;
enum class FrameSRV : u32 // Register offset from REG_FRAME_SRV_BASE (must match GlobalLayouts.slang)
{
    Meshes              = 0, // t0: gMeshes
    Materials           = 1, // t1: gMaterials
    Instances           = 2, // t2: gInstances
    Lights              = 3, // t3: gLights
    Envs                = 4, // t4: gEnvs
    InstanceRedirection = 5, // t5: gInstanceRedirection
    Textures            = 6, // t6: gTextures
    ParticleSystems     = 7, // t7: gParticleSystems
    Count
};
constexpr u32 REG_FRAME_SRV_COUNT = (u32)FrameSRV::Count;

// Space 1: Pass I/O (declared by each pass shader with DECLARE_* macros). Register map owned by Graphics::PassIO
constexpr u32 REG_PASS_SLOT_COUNT      = Graphics::PassIO::SLOT_COUNT;
constexpr u32 REG_PASS_SRV_IMAGE_BASE  = Graphics::PassIO::texture( 0 );  // t0 - t7
constexpr u32 REG_PASS_SRV_BUFFER_BASE = Graphics::PassIO::buffer( 0 );   // t8 - t15
constexpr u32 REG_PASS_UAV_IMAGE_BASE  = Graphics::PassIO::rwTexture( 0 ); // u0 - u7
constexpr u32 REG_PASS_UAV_BUFFER_BASE = Graphics::PassIO::rwBuffer( 0 );  // u8 - u15

// Space 2: Push constants (declared by each pass / material shader)
constexpr u32 REG_PUSH_CONSTANT_BASE = 0; // b0

//-----------------------------------------------------------------------------
// Geometry page split (% of MemoryBudget::minGeometryPageAlloc). Estimated from bytes per triangle:
// vertex 64 B (~0.55/tri), index 12 B, meshlet 48 B (~1/110 tris), meshlet vertices ~2.3 B, meshlet primitives ~3 B
//-----------------------------------------------------------------------------
constexpr u64 GEOMETRY_BUDGET_VERTICES_PCT           = 64;
constexpr u64 GEOMETRY_BUDGET_INDICES_PCT            = 22;
constexpr u64 GEOMETRY_BUDGET_MESHLETS_PCT           = 2;
constexpr u64 GEOMETRY_BUDGET_MESHLET_VERTICES_PCT   = 6;
constexpr u64 GEOMETRY_BUDGET_MESHLET_PRIMITIVES_PCT = 6;
AXION_STATIC_ASSERT( GEOMETRY_BUDGET_VERTICES_PCT + GEOMETRY_BUDGET_INDICES_PCT + GEOMETRY_BUDGET_MESHLETS_PCT +
                             GEOMETRY_BUDGET_MESHLET_VERTICES_PCT + GEOMETRY_BUDGET_MESHLET_PRIMITIVES_PCT ==
                         100,
                     "Geometry budget split must add up to 100%" );

//-----------------------------------------------------------------------------
// Rasterizer Caps
//-----------------------------------------------------------------------------
// Mesh shading dispatch limits (D3D12: groups per grid dimension and total groups per DispatchMesh)
constexpr u32 MAX_MESH_DISPATCH_GROUPS_PER_DIMENSION = 65535;
constexpr u32 MAX_MESH_DISPATCH_GROUPS               = 1u << 22;

// Meshlets culled per amplification group in the vis pass (must match AS_GROUP_SIZE in VisPass.slang)
constexpr u32 VIS_AMPLIFICATION_GROUP_SIZE = 32;

// Material binning: one bin per resolve PSO of the frame. One tile thread flushes each bin of the tile histogram
constexpr u32 BINNING_TILE_SIZE      = 8; ///< Must match BINNING_TILE_SIZE in Constants.slang
constexpr u32 MAX_RESOLVE_BINS       = BINNING_TILE_SIZE * BINNING_TILE_SIZE;
constexpr u32 RESOLVE_BIN_GROUP_SIZE = 64;   ///< Pixels of the bin list per resolve group (must match Constants.slang)
constexpr u32 BIN_NONE               = 0xFF; ///< Pixel or bundle without bin: background, or no resolve PSO (must match Constants.slang)
AXION_STATIC_ASSERT( MAX_RESOLVE_BINS < BIN_NONE, "Bins live in a R8_UINT image where BIN_NONE marks the background" );

constexpr u64 MAX_SHADER_RESOURCE_VIEWS = 16384;
constexpr u64 MAX_SAMPLER_VIEWS         = 512;

constexpr u64 MAX_GLOBAL_UBO_BYTES     = 1024;
constexpr u64 MAX_PUSH_CONSTANTS_BYTES = 128;

constexpr u32 PARTICLE_DATA_STRIDE = 48; ///< Bytes per simulated particle in a particle buffer (must match the simulation shader)

constexpr const char* SHADER_CACHE_NAME = "RasterizerShaders"; ///< Prefix of the rasterizer shader cache files

//-----------------------------------------------------------------------------
// Rasterizer Config
//-----------------------------------------------------------------------------
Graphics::PipelineLayoutHandle buildGlobalLayout( Graphics::IPipelineRegistry& pip );

void matLibConfig( Graphics::PipelineLayoutHandle globalLayoutHandle,
                   RasterizerSettings&            settings,
                   MaterialLibraryDesc&           matLibDesc );

/// @brief Registers every material archetype known by the engine. Shared by the rasterizer and its shader cache compiler.
void registerMaterials( MaterialLibrary<(u32)MaterialPassType::Count>& matLib );

/// @brief Declares the render passes of the rasterizer. Shared by the rasterizer and its shader cache compiler.
void registerPasses( PassManager& passes );

// void createDefaultResources( Renderer* rnd, GPUResources& outRes );

} // namespace Config
} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END
