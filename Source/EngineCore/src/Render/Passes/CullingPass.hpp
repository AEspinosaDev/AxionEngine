#pragma once
#include <Render/DrawIndirect.h>
#include <Render/GPUScene.h>
#include <Render/PassManager.h>
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class CullingPass : public IRenderPass
{
public:
    struct Config {

        Graphics::RGResourceHandle outIndirectBufferHandle;
        Graphics::RGResourceHandle outCulledRedirectBufferHandle;

        Graphics::BufferSlice inFrameSlice;
        Graphics::BufferSlice inMeshesSlice;
        Graphics::BufferSlice inInstancesSlice;
        Graphics::BufferSlice inRedirectionSlice;

        IndirectCommandPayload indirectData;

        u32 instanceCount;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders
                        .shader( "CullingShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/Culling.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "computeMain" )
                        .load();

        // Thanks to slang auto reflect layoutis automatically generated
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "CullingPipeline" ).shader( "CullingShader" ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {

        builder.addPass<Config>( "CullingPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.outIndirectBufferHandle = pb.write(data.outIndirectBufferHandle,  Graphics::RHI::ResourceState::UnorderedAccess);
        data.outCulledRedirectBufferHandle    = pb.write(data.outCulledRedirectBufferHandle,  Graphics::RHI::ResourceState::UnorderedAccess); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd = ctx.cmd;

        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* pipLayout = pso->getDescription().layout;
        // BINDING
        ctx.cmd->bindComputePipeline( pso );

        // SET 0: INPUTS (ReadOnly) -> Space 0
        auto* set0 = ctx.allocateSet( pipLayout, 0 );

        set0->attachBufferSlice( 0, data.inFrameSlice, Graphics::RHI::ResourceState::ConstantBuffer );
        set0->attachBufferSlice( 1, data.inMeshesSlice, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferSlice( 2, data.inInstancesSlice, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferSlice( 3, data.inRedirectionSlice, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferSlice( 4, data.indirectData.batchMapSlice, Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 0, set0 );

        // SET 1: OUTPUTS (ReadWrite / UAV) -> Space 1
        auto* set1 = ctx.allocateSet( pipLayout, 1 );

        auto* indirectCmdBufer     = ctx.getBuffer( data.outIndirectBufferHandle );
        auto* culledRedirectBuffer = ctx.getBuffer( data.outCulledRedirectBufferHandle );
        set1->attach( 0, indirectCmdBufer, Graphics::RHI::ResourceState::UnorderedAccess );
        set1->attach( 1, culledRedirectBuffer, Graphics::RHI::ResourceState::UnorderedAccess );

        cmd->bindDescriptorSet( 1, set1 );

        cmd->pushConstants( 2, data.instanceCount );

        // DISPATCH

        u32 groupSize  = 64;
        u32 groupCount = ( data.instanceCount + groupSize - 1 ) / groupSize;
        ctx.cmd->dispatch( { groupCount, 1, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END