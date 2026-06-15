#pragma once
#include <Render/PassManager.h>
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class VisResolvePass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inVisHandle;
        Graphics::RGResourceHandle outColorHandle;
        u32                        workgroupSize = 8;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "Vis Resolve Naive" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/VisBuffer/VisResolveNaive.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csResolve" )
                        .autoReflect( false )
                        .load();
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _layHandle = pipelines.layout( "NaiveResolve_Layout" )
                         // Space 0: Persistent (Geometry, Materials and Textures)
                         .addSet( {
                             { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Vertex
                             { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Index
                             { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Materials
                             { 3, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 8192 },       // Textures
                             { 0, Graphics::RHI::DescriptorType::Sampler, Graphics::RHI::ShaderStage::All, 128 }              // Samplers
                         } )
                         // Space 1: Scene Data
                         .addSet( {
                             { 0, Graphics::RHI::DescriptorType::UniformBuffer, Graphics::RHI::ShaderStage::All, 1 },         // Frame
                             { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Meshes
                             { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Material Metadata
                             { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Instances
                             { 3, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Lights
                             { 4, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Environments
                             { 5, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }  // Instance Redirection Buffer
                         } )
                         // Space 2: Instance ID Push Constant
                         .setPushConstants( sizeof( u32 ), 0, 2 )
                         .addSet( {
                             { 0, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::Compute, 1 }, // Frame
                             { 1, Graphics::RHI::DescriptorType::StorageImage, Graphics::RHI::ShaderStage::Compute, 1 }, // Meshes
                         } )
                         .enableIndirectRendering()
                         .create();

        _pipHandle = pipelines.compute( "Vis Resolve Naive PSO" )
                         .shader( _shHandle )
                         .create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Config&                 config ) {

        builder.addPass<Config>( "Vis Resolve Naive Pass", config, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.inVisHandle  = pb.read( data.inVisHandle );
                data.outColorHandle = pb.write( data.outColorHandle ); }, [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = ctx.pipelines.getLayout( _layHandle );
            
        auto* texIn  = ctx.getTexture( data.inVisHandle );
        auto* texOut = ctx.getTexture( data.outColorHandle );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

         // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------

        auto* set0 = ctx.allocateSet( layout, 0 );
        
        set0->attach( 0, r.getBuffer( _res.vertexBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, r.getBuffer( _res.indexBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 2, r.getBuffer( _res.mtlBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBindlessArray( 3, 0, initialTextures, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBindlessArray( 0, 0, initialSamplers );
        
        cmd->bindDescriptorSet( 0, set0, layout );
        
        // SPACE 1: Volatile Data (Slices into the giant UBO)
        auto* set1 = ctx.allocateSet( layout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3), Redirection (t4)
        set1->attachBufferSlice( 0, data.inFrameSlice, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachBufferSlice( 1, data.inMeshesSlice, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferSlice( 2, data.inMaterialsSlice, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferSlice( 3, data.inInstancesSlice, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferSlice( 4, data.inLightsSlice, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferSlice( 5, data.inEnvsSlice, Graphics::RHI::ResourceState::ShaderResource );


        if ( data.useGPUCulling )
        {
            auto* culledBuf = ctx.getBuffer( data.inCulledRedirectBufferHandle );

            Graphics::BufferSlice culledSlice;
            culledSlice.container = culledBuf;
            culledSlice.offset    = 0;
            culledSlice.size      = data.inRedirectionSlice.size;
            culledSlice.stride    = data.inRedirectionSlice.stride;
            set1->attachBufferSlice( 6, culledSlice, Graphics::RHI::ResourceState::ShaderResource );
        } else
            set1->attachBufferSlice( 6, data.inRedirectionSlice, Graphics::RHI::ResourceState::ShaderResource );

         cmd->bindDescriptorSet( 1, set1, layout );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        auto size = texIn->getDescription().size;
        u32  wgs  = data.workgroupSize;
        ctx.cmd->dispatch( { ( size.width + wgs - 1 ) / wgs, ( size.height + wgs - 1 ) / wgs, 1 } );
    }

    Graphics::PipelineHandle       _pipHandle;
    Graphics::PipelineLayoutHandle _layHandle;
    Graphics::ShaderHandle         _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END