#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Core/Render/IRasterizer.h>
#include <Axion/Graphics/ShaderCommon.h>

#include <Render/MaterialLibrary.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer {
namespace Config {

//-----------------------------------------------------------------------------
// Rasterizer Material Passes
//-----------------------------------------------------------------------------
enum class MaterialPassType : u32
{
    Visibility        = 0,
    VisibilityResolve = 1,
    // Shadow             = 2,
    // ForwardTransparent = 3,

    Count
};

//-----------------------------------------------------------------------------
// Rasterizer Layout Sets
//-----------------------------------------------------------------------------
enum class LayoutSetType : u32
{
    Persistent = 0,
    FrameBound = 1,
    PassBound  = 2,

    Count,
    PushConstant = Count
};

//-----------------------------------------------------------------------------
// Rasterizer Layout Register Map (must match GlobalLayouts.slang)
//-----------------------------------------------------------------------------

// Space 0: Persistent global buffers (textures & samplers are bindless, heap indexed)
constexpr u32 REG_GLOBAL_BUFFERS_BASE  = 0; // t0: gGlobalVertices, t1: gGlobalIndices, t2: gGlobalMaterials
constexpr u32 REG_GLOBAL_BUFFERS_COUNT = 3;

// Space 1: Frame data
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
    Count
};
constexpr u32 REG_FRAME_SRV_COUNT = (u32)FrameSRV::Count;

// Space 2: Pass I/O (declared by each pass shader with DECLARE_* macros). Register map owned by Graphics::PassIO
constexpr u32 REG_PASS_SLOT_COUNT      = Graphics::PassIO::SLOT_COUNT;
constexpr u32 REG_PASS_SRV_IMAGE_BASE  = Graphics::PassIO::texture( 0 );  // t0 - t7
constexpr u32 REG_PASS_SRV_BUFFER_BASE = Graphics::PassIO::buffer( 0 );   // t8 - t15
constexpr u32 REG_PASS_UAV_IMAGE_BASE  = Graphics::PassIO::rwTexture( 0 ); // u0 - u7
constexpr u32 REG_PASS_UAV_BUFFER_BASE = Graphics::PassIO::rwBuffer( 0 );  // u8 - u15

// Space 3: Push constants (declared by each pass / material shader)
constexpr u32 REG_PUSH_CONSTANT_BASE = 0; // b0

//-----------------------------------------------------------------------------
// Rasterizer Caps
//-----------------------------------------------------------------------------
constexpr u64 MAX_SHADER_RESOURCE_VIEWS = 16384;
constexpr u64 MAX_SAMPLER_VIEWS         = 512;

constexpr u64 MAX_GLOBAL_UBO_BYTES     = 1024;
constexpr u64 MAX_PUSH_CONSTANTS_BYTES = 128;

//-----------------------------------------------------------------------------
// Rasterizer Config
//-----------------------------------------------------------------------------
Graphics::PipelineLayoutHandle buildGlobalLayout( Graphics::IPipelineRegistry& pip );

void matLibConfig( Graphics::PipelineLayoutHandle globalLayoutHandle,
                   RasterizerSettings&            settings,
                   MaterialLibraryDesc&           matLibDesc );

// void createDefaultResources( Renderer* rnd, GPUResources& outRes );

} // namespace Config
} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END
