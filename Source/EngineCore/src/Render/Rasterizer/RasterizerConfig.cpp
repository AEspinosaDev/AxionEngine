#include "RasterizerConfig.h"
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

namespace Rasterizer::Config {

Graphics::PipelineLayoutHandle Rasterizer::Config::buildGlobalLayout( Graphics::IPipelineRegistry& pip ) {
    return pip.layout( "Rasterizer_Global_Layout" )
        // Space 0: Persistent Bindless (Geometry, Materials, Textures)
        .addSet( {
            { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                   // t0: gGlobalVertices
            { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                   // t1: gGlobalIndices
            { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                   // t2: gGlobalMaterials
            { 3, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_2D_TEXTURES },   // t3: gTextures2D
            { 4, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_3D_TEXTURES },   // t4: gTextures3D
            { 5, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_CUBE_TEXTURES }, // t5: gTexturesCube
            { 0, Graphics::RHI::DescriptorType::Sampler, Graphics::RHI::ShaderStage::All, MAX_PERSISTENT_SAMPLERS }            // s0: gSamplers
        } )
        // Space 1: Frame Transient Bindless (Scene Data Per-Frame)
        .addSet( {
            { 0, Graphics::RHI::DescriptorType::UniformBuffer, Graphics::RHI::ShaderStage::All, 1 },         // b0: gFrame
            { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // t0: gMeshes
            { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // t1: gMaterials
            { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // t2: gInstances
            { 3, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // t3: gLights
            { 4, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // t4: gEnvs
            { 5, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }  // t5: gInstanceRedirection
        } )
        // Space 2: Transient I/O (Per-Pass)
        .addSet( {
            // Input (Read-Only)
            { 0, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t0: gPassRead2D_F4
            { 1, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t1: gPassRead2D_F1
            { 2, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t2: gPassRead2D_U2
            { 3, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t3: gPassRead2D_U1
            { 4, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t4: gPassRead3D_F4
            { 5, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t5: gPassRead3D_F1
            { 6, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES },          // t6: gPassReadCube_F4
            { 7, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // t7: gPassReadBuffer

            // Output (Read-Write)
            { 0, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // u0: gPassWrite2D_F4
            { 1, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // u1: gPassWrite2D_F1
            { 2, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // u2: gPassWrite2D_U2
            { 3, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // u3: gPassWrite3D_F4
            { 4, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES }, // u4: gPassWrite3D_F1
            { 5, Graphics::RHI::DescriptorType::StorageBuffer, Graphics::RHI::ShaderStage::All, MAX_TRANSIENT_IO_RESOURCES } // u5: gPassWriteBuffer
        } )
        // Space 3: Universal Push Constants
        // Maximum guaranteed size across all hardware is 128 bytes.
        .setPushConstants( MAX_PUSH_CONSTANTS_BYTES, 0, 3 )
        .enableIndirectRendering()
        .create();
}
void matLibConfig( Graphics::PipelineLayoutHandle globalLayoutHandle, RasterizerSettings& settings, MaterialLibrary<+MaterialPassType::Count>& matLib ) {

    // Visibility pass
    matLib.setPassProfile( +MaterialPassType::Visibility, MaterialPassProfile {
                               .name                = "Visibility",
                               .layoutHandle        = globalLayoutHandle,
                               .bindPointType       = Graphics::RHI::PipelineBindPoint::Graphic,
                                 pass.entryPoints = {
        { "vsForward", Axion::Graphics::ShaderType::Vertex },
        { "psForward", Axion::Graphics::ShaderType::Pixel } };
    pass.customIncludePath = AXION_SHADER_DIR "/Slang/BxDFs";
                               .renderTargetFormats = { Graphics::Format::RG32_UINT, Graphics::Format::RG16_FLOAT },
                               .depthTargetFormat   = settings.depthFormat, } );

    // Resolve pass
    matLib.setPassProfile( +MaterialPassType::VisibilityResolve,
                           MaterialPassProfile {
                               .name          = "VisibilityResolve",
                               .layoutHandle  = globalLayoutHandle,
                               .bindPointType = Graphics::RHI::PipelineBindPoint::Compute,
                           } );
}

// void createDefaultResources( Renderer* rnd, GPUResources& outRes ) {
// }

} // namespace Rasterizer::Config

} // namespace Core::Render

AXION_NAMESPACE_END
