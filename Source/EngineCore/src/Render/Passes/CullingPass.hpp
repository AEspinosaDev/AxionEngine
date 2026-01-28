#pragma once
#include "../DrawIndirect.h"
#include "../GPUScene.h"
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class CullingPass : public IRenderPass
{
public:
    struct Config {

        Graphics::RGResourceHandle outIndirectCmdBufferHandle;
        Graphics::RGResourceHandle outCulledRedirectBufferHandle;

        Graphics::RHI::BufferView inFrameView;
        Graphics::RHI::BufferView inMeshesView;
        Graphics::RHI::BufferView inInstancesView;
        Graphics::RHI::BufferView inRedirectionView;
        Graphics::RHI::BufferView inIndirectCmdMapView;

        uint instanceCount;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders
                        .shader( "CullingShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocessing/Culling.slang" )
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
        data.outIndirectCmdBufferHandle = pb.write(data.outIndirectCmdBufferHandle,  Graphics::RHI::ResourceState::UnorderedAccess);
        data.outCulledRedirectBufferHandle    = pb.write(data.outCulledRedirectBufferHandle,  Graphics::RHI::ResourceState::UnorderedAccess); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd = ctx.cmd;
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* pipLayout = pso->getDescription().layout; //???

        // SET 0: INPUTS (ReadOnly) -> Space 0
        auto* set0 = ctx.allocateSet( pipLayout, 0 );

        set0->attachBufferView( 0, data.inFrameView, Graphics::RHI::ResourceState::ConstantBuffer );
        set0->attachBufferView( 1, data.inMeshesView, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferView( 2, data.inInstancesView, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferView( 3, data.inRedirectionView, Graphics::RHI::ResourceState::ShaderResource );
        set0->attachBufferView( 4, data.inIndirectCmdMapView, Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 0, set0, pipLayout );

        // SET 1: OUTPUTS (ReadWrite / UAV) -> Space 1
        auto* set1 = ctx.allocateSet( pipLayout, 1 );

        auto* indirectCmdBufer     = ctx.getBuffer( data.outIndirectCmdBufferHandle );
        auto* culledRedirectBuffer = ctx.getBuffer( data.outCulledRedirectBufferHandle );
        set1->attach( 0, indirectCmdBufer, Graphics::RHI::ResourceState::UnorderedAccess );
        set1->attach( 1, culledRedirectBuffer, Graphics::RHI::ResourceState::UnorderedAccess );

        cmd->bindDescriptorSet( 1, set1 );

        // DISPATCH
        ctx.cmd->bindComputePipeline( pso );

        uint groupSize  = 64;
        uint groupCount = ( data.instanceCount + groupSize - 1 ) / groupSize;
        ctx.cmd->dispatch( { groupCount, 1, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END