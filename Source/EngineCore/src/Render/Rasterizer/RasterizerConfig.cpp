#include "RasterizerConfig.h"
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

namespace Rasterizer::Config {

Graphics::PipelineLayoutHandle Rasterizer::Config::buildGlobalLayout( Graphics::IPipelineRegistry& pip ) {
    return pip.layout( "Rasterizer_Global_Layout" )
        // Space 0: Persistent global buffers. Textures & samplers are bindless (heap indexed, no bindings)
        .addSet( {
            { { .base = REG_GLOBAL_BUFFERS_BASE, .count = REG_GLOBAL_BUFFERS_COUNT }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // t0: gGlobalVertices, t1: gGlobalIndices, t2: gGlobalMaterials
        } )
        // Space 1: Frame Transient (Scene Data Per-Frame)
        .addSet( {
            { { .base = REG_FRAME_CBV_BASE, .count = 1 }, Graphics::RHI::DescriptorType::CBV, Graphics::RHI::ShaderStage::All, 1 },                         // b0: gFrame
            { { .base = REG_FRAME_SRV_BASE, .count = REG_FRAME_SRV_COUNT }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // t0: gMeshes t1: gMaterials t2: gInstances t3: gLights t4: gEnvs t5: gInstanceRedirection
        } )
        // Space 2: Transient I/O (Per-Pass, declared by each pass shader)
        .addSet( {
            // Input (Read-Only)
            { { .base = REG_PASS_SRV_IMAGE_BASE, .count = REG_PASS_SLOT_COUNT }, Graphics::RHI::DescriptorType::SRV_Image, Graphics::RHI::ShaderStage::All, 1 },   // t0 - t7
            { { .base = REG_PASS_SRV_BUFFER_BASE, .count = REG_PASS_SLOT_COUNT }, Graphics::RHI::DescriptorType::SRV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // t8 - t15
            // Output (Read-Write)
            { { .base = REG_PASS_UAV_IMAGE_BASE, .count = REG_PASS_SLOT_COUNT }, Graphics::RHI::DescriptorType::UAV_Image, Graphics::RHI::ShaderStage::All, 1 },   // u0 - u7
            { { .base = REG_PASS_UAV_BUFFER_BASE, .count = REG_PASS_SLOT_COUNT }, Graphics::RHI::DescriptorType::UAV_Buffer, Graphics::RHI::ShaderStage::All, 1 }, // u8 - u15
        } )
        // Space 3: Universal Push Constants (declared by each pass / material shader)
        // Maximum guaranteed size across all hardware is 128 bytes.
        .setPushConstants( MAX_PUSH_CONSTANTS_BYTES, REG_PUSH_CONSTANT_BASE, (u32)LayoutSetType::PushConstant )
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
