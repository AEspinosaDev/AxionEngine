#pragma once
#include "../DrawIndirect.h"
#include "../MaterialSystem.h"
#include "../PassSystem.h"
#include <Axion/Graphics/Subsystems/IRenderGraph.h>

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
        // In Buffers
        GlobalBufferHandles inGlobalBufferHandles;

        Graphics::BufferSlice inFrameSlice;
        Graphics::BufferSlice inMeshesSlice;
        Graphics::BufferSlice inInstancesSlice;
        Graphics::BufferSlice inRedirectionSlice;

        IndirectCommandPayload     indirectData;
        Graphics::RGResourceHandle inIndirectBufferHandle;
        Graphics::RGResourceHandle inCulledRedirectBufferHandle;
        bool                       useGPUCulling = false;

        // Out RTOs
        Graphics::RGResourceHandle outVisHandle;
        Graphics::RGResourceHandle outVelocityHandle;
        Graphics::RGResourceHandle outDepthHandle;

        // Resources
        Graphics::RHI::IDescriptorSet* persistentDescriptorSet = nullptr;

        MaterialLibrary*               matLib;
        Graphics::PipelineLayoutHandle matLayoutHandle;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {
        builder.addPass<Config>( "Vis Pass", seedData, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                                    //RTOs
                                    data.outVisHandle        = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::RenderTarget );
                                    data.outVelocityHandle  = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::RenderTarget );
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

        // RenderTargets
        auto* rto0 = ctx.getTexture( data.outVisHandle );
        auto* rto1 = ctx.getTexture( data.outVelocityHandle );
        auto* dsv  = ctx.getTexture( data.outDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( data.matLayoutHandle );

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

        // SPACE 1: Volatile Data (Slices into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3), Redirection (t4)
        set1->attachBufferSlice( 0, data.inFrameSlice, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachBufferSlice( 1, data.inMeshesSlice, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferSlice( 3, data.inInstancesSlice, Graphics::RHI::ResourceState::ShaderResource );

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

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

        auto* ib = ctx.getBuffer( data.inGlobalBufferHandles.index );
        cmd->bindIndexBuffer( ib );
        auto* indirectBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferSlice.container;

        //Cache PSO
        Graphics::PipelineHandle currentPsoHandle;
        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            // auto topologyType = (MaterialTopologyType)toGFXTopology( ()batch.topologyID );
            Graphics::PipelineHandle psoHandle = matArchetypes[batch.archetypeID].getPipeline(
                MaterialPassType::Visibility, TopologyType::Triangles );

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