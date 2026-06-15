#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Core/Render/IRasterizer.h>

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
// Rasterizer Caps
//-----------------------------------------------------------------------------
constexpr u64 MAX_SHADER_RESOURCE_VIEWS = 16384;
constexpr u64 MAX_SAMPLER_VIEWS         = 512;

constexpr u64 MAX_PERSISTENT_2D_TEXTURES   = 8192;
constexpr u64 MAX_PERSISTENT_3D_TEXTURES   = 8;
constexpr u64 MAX_PERSISTENT_CUBE_TEXTURES = 32;
constexpr u64 MAX_PERSISTENT_SAMPLERS      = 128;

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
