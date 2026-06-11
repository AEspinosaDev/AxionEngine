#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Core/Render/IRasterizer.h>

#include <Render/MaterialLibrary.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer {
namespace Config {

// Material Pass types supported by the rasterizer,
// these are used for pipeline permutations in material system
enum class MaterialPassType : u32
{
    Visibility        = 0,
    VisibilityResolve = 1,
    // Shadow             = 2,
    // ForwardTransparent = 3,

    Count
};

constexpr u32 operator+( MaterialPassType e ) noexcept {
    return static_cast<u32>( e );
}

constexpr u64 MAX_PERSISTENT_2D_TEXTURES   = 8192;
constexpr u64 MAX_PERSISTENT_3D_TEXTURES   = 8;
constexpr u64 MAX_PERSISTENT_CUBE_TEXTURES = 32;
constexpr u64 MAX_PERSISTENT_SAMPLERS      = 128;
constexpr u64 MAX_TRANSIENT_IO_RESOURCES   = 8;

constexpr u64 MAX_GLOBAL_UBO_BYTES     = 1024;
constexpr u64 MAX_PUSH_CONSTANTS_BYTES = 128;

Graphics::PipelineLayoutHandle buildGlobalLayout( Graphics::IPipelineRegistry& pip );
void                           matLibConfig( Graphics::PipelineLayoutHandle globalLayoutHandle,
                                             RasterizerSettings&            settings,
                                             MaterialLibraryDesc&           matLibDesc );

// void createDefaultResources( Renderer* rnd, GPUResources& outRes );

} // namespace Config
} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END
