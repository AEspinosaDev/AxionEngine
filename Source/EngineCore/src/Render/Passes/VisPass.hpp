#pragma once
#include <Axion/Graphics/Subsystems/IRenderGraph.h>
#include <Render/DrawIndirect.h>
#include <Render/MaterialLibrary.h>
#include <Render/PassManager.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class VisPass : public IRenderPass
{
public:
    struct GlobalBufferHandles {
        Graphics::RGResourceHandle vertex;
        Graphics::RGResourceHandle index;
    };

    struct Config {
        IndirectCommandPayload indirectData;

        // In Buffers
        GlobalBufferHandles        inGlobalBufferHandles;
        Graphics::RGResourceHandle inIndirectBufferHandle;
        Graphics::RGResourceHandle inCulledRedirectBufferHandle;
        bool                       useGPUCulling = false;

        // Out RTOs
        Graphics::RGResourceHandle outVisHandle;
        Graphics::RGResourceHandle outVelocityHandle;
        Graphics::RGResourceHandle outDepthHandle;

        // Resources
        Graphics::RHI::IDescriptorSet* persistentDescriptorSet = nullptr;
        Graphics::RHI::IDescriptorSet* transientDescriptorSet  = nullptr;

        u32               materialPassSlot;
        IMaterialLibrary* matLib;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {
        builder.addPass<Config>( "Vis Pass", seedData, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                                    //RTOs
                                    data.outVisHandle        = pb.write( data.outVisHandle, Graphics::RHI::ResourceState::RenderTarget );
                                    data.outVelocityHandle  = pb.write( data.outVelocityHandle, Graphics::RHI::ResourceState::RenderTarget );
                                    data.outDepthHandle     = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::DepthWrite );
                                    //Global
                                    data.inGlobalBufferHandles.vertex   = pb.read( data.inGlobalBufferHandles.vertex, Graphics::RHI::ResourceState::ShaderResource );
                                    data.inGlobalBufferHandles.index    = pb.read( data.inGlobalBufferHandles.index, Graphics::RHI::ResourceState::ShaderResource );
                                    //Indirect
                                    if(data.useGPUCulling){
                                        data.inIndirectBufferHandle       = pb.read( data.inIndirectBufferHandle, Graphics::RHI::ResourceState::IndirectArgument );
                                        data.inCulledRedirectBufferHandle = pb.read( data.inCulledRedirectBufferHandle, Graphics::RHI::ResourceState::ShaderResource );
                                    } },

                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd = ctx.cmd;

        // RenderMaterial
        const MaterialPassProfile& matPassProfile = data.matLib->getPassProfile( data.materialPassSlot );

        // RenderTargets
        auto* rto0 = ctx.getTexture( data.outVisHandle );
        auto* rto1 = ctx.getTexture( data.outVelocityHandle );
        auto* dsv  = ctx.getTexture( data.outDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( matPassProfile.layoutHandle );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea = rto0->getDescription().size.to2D();
        info.colorAttachments.resize( 2 );
        info.colorAttachments[0]    = { .texture = rto0 };
        info.colorAttachments[1]    = { .texture = rto1 };
        info.depthStencilAttachment = { .texture = dsv };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------

        auto* set0 = data.persistentDescriptorSet;
        cmd->bindDescriptorSet( 0, set0, matLayout );

        auto* set1 = data.transientDescriptorSet;
        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------

        auto* ib = ctx.getBuffer( data.inGlobalBufferHandles.index );
        cmd->bindIndexBuffer( ib );
        auto* indirectBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferSlice.container;

        // Cache PSO
        Graphics::PipelineHandle currentPsoHandle;
        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( batch.psoID, data.materialPassSlot );

            if ( currentPsoHandle != psoHandle )
            {
                auto* pso = ctx.pipelines.getGraphicPipeline( psoHandle );
                cmd->bindGraphicPipeline( pso );
                currentPsoHandle = psoHandle;
            }

            if ( psoHandle.isValid() )
            {
                cmd->drawIndexedIndirect(
                    indirectBuffer,
                    batch.bufferOffset,
                    batch.drawCount );
            }
        }

        cmd->endRendering();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END