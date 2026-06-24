#include "RasterizerConfig.h"
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

namespace Rasterizer::Config {

Graphics::PipelineLayoutHandle Rasterizer::Config::buildGlobalLayout( Graphics::IPipelineRegistry& pip ) {
    return pip.layout( "Rasterizer_Global_Layout" )
        // Space 0: Persistent + Bindless (Geometry, Materials, Textures)
        .addSet( {
            { { .base = 0, .count = 3 }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 },                   // t0: gGlobalVertices, t1: gGlobalIndices,  t2: gGlobalMaterials
            { { .base = 3, .count = 1 }, Graphics::RHI::DescriptorType::SRV_Image, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_2D_TEXTURES },   // t3: gTextures2D
            { { .base = 4, .count = 1 }, Graphics::RHI::DescriptorType::SRV_Image, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_3D_TEXTURES },   // t4: gTextures3D
            { { .base = 5, .count = 1 }, Graphics::RHI::DescriptorType::SRV_Image, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_CUBE_TEXTURES }, // t5: gTexturesCube
            { { .base = 0, .count = 1 }, Graphics::RHI::DescriptorType::Sampler, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_SAMPLERS }            // s0: gSamplers
        } )
        // Space 1: Frame Transient Bindless (Scene Data Per-Frame)
        .addSet( {
            { { .base = 0, .count = 1 }, Graphics::RHI::DescriptorType::CBV, Graphics::RHI::ShaderStage::All, 1 },         // b0: gFrame
            { { .base = 0, .count = 6 }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // t0: gMeshes t1: gMaterials t2: gInstances t3: gLights t4: gEnvs t5: gInstanceRedirection
        } )
        // Space 2: Transient I/O (Per-Pass)
        .addSet( {
            // Input (Read-Only) 
            { { .base = 0, .count = 8 }, Graphics::RHI::DescriptorType::SRV_Image,          Graphics::RHI::ShaderStage::All, 1 }, // t0 - t7  
            { { .base = 8, .count = 8 }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // t8 - t15 
            // Output (Read-Write)
            { { .base = 0, .count = 8 }, Graphics::RHI::DescriptorType::UAV_Image,          Graphics::RHI::ShaderStage::All, 1 }, // u0 - u7  
            { { .base = 8, .count = 8 }, Graphics::RHI::DescriptorType::UAV_Buffer,         Graphics::RHI::ShaderStage::All, 1 }, // u8 - u15 
        } )
        // Space 3: Universal Push Constants
        // Maximum guaranteed size across all hardware is 128 bytes.
        .setPushConstants( MAX_PUSH_CONSTANTS_BYTES, 0, 3 )
        .enableIndirectRendering()
        .create();
}
void matLibConfig( Graphics::PipelineLayoutHandle globalLayoutHandle, RasterizerSettings& settings, MaterialLibraryDesc& matLibDesc ) {

    // Set API
    matLibDesc.gfxApi = settings.common.gfxApi;

    // Visibility pass
    matLibDesc.passProfiles.pushBack( MaterialPassProfile {
        .name          = "Visibility",
        .layoutHandle  = globalLayoutHandle,
        .bindPointType = Graphics::RHI::PipelineBindPoint::Graphic,
        // Pass shader
        .shaderPath          = AXION_SHADER_DIR "/Slang/VisibilityPass.slang",
        .shaderIncludePath   = AXION_SHADER_DIR "/Slang/BxDFs",
        .entryPoints         = { { "vsVis", Axion::Graphics::ShaderType::Vertex },
                                 { "psVis", Axion::Graphics::ShaderType::Pixel } },
        .needsSpecialization = true,
        // Pass fmts
        .renderTargetFormats = { Graphics::Format::RG32_UINT,
                                 Graphics::Format::RG16_FLOAT },
        .depthTargetFormat   = settings.depthFormat,
        // Pass state
        .defaultState = {},
        .overrideMask = StateOverrideFlags::Topology | StateOverrideFlags::CullMode } );

    // Resolve pass
    matLibDesc.passProfiles.pushBack( MaterialPassProfile {
        .name          = "VisibilityResolve",
        .layoutHandle  = globalLayoutHandle,
        .bindPointType = Graphics::RHI::PipelineBindPoint::Compute,
        // Pass shader
        .shaderPath          = AXION_SHADER_DIR "/Slang/VisResolveNaivePass.slang",
        .shaderIncludePath   = AXION_SHADER_DIR "/Slang/BxDFs",
        .entryPoints         = { { "csResolve", Axion::Graphics::ShaderType::Compute } },
        .needsSpecialization = true,
        // Pass fmts
        .renderTargetFormats = { Graphics::Format::RGBA16_FLOAT },
        .depthTargetFormat   = settings.depthFormat,
        // Pass state
        .defaultState = {},
        .overrideMask = StateOverrideFlags::All } );
}

// void createDefaultResources( Renderer* rnd, GPUResources& outRes ) {
// }

} // namespace Rasterizer::Config

} // namespace Core::Render

AXION_NAMESPACE_END
