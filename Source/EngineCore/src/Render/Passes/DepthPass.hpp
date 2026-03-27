#pragma once
#include "../DrawIndirect.h"
#include "../MaterialSystem.h"
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class DepthPrePass : public IRenderPass
{
public:
    struct GlobalBufferHandles {
        Graphics::RGResourceHandle vertex;
        Graphics::RGResourceHandle index;
        Graphics::RGResourceHandle material;
    };

    struct Config {
        GlobalBufferHandles inGlobalBufferHandles;

        Graphics::RHI::BufferView inFrameView;
        Graphics::RHI::BufferView inMeshesView;
        Graphics::RHI::BufferView inMaterialsView;
        Graphics::RHI::BufferView inInstancesView;
        Graphics::RHI::BufferView inLightsView;
        Graphics::RHI::BufferView inEnvsView;
        Graphics::RHI::BufferView inRedirectionView;

        IndirectCommandData        indirectData;
        Graphics::RGResourceHandle inIndirectBufferHandle;
        Graphics::RGResourceHandle inCulledRedirectBufferHandle;
        bool                       useGPUCulling = false;

        Graphics::RHI::IDescriptorSet* persistentDescriptorSet = nullptr;

        MaterialLibrary*               matLib;
        Graphics::PipelineLayoutHandle matLayoutHandle;

        Graphics::RGResourceHandle outDepthHandle;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        /* NO OP */
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        /* NO OP */
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {
        builder.addPass<Config>( "DepthPrePass", seedData, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                                    //Rendertargets
                                     data.outDepthHandle = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::DepthWrite );
                                    //Global
                                     data.inGlobalBufferHandles.vertex   = pb.read( data.inGlobalBufferHandles.vertex, Graphics::RHI::ResourceState::ShaderResource );
                                     data.inGlobalBufferHandles.index    = pb.read( data.inGlobalBufferHandles.index, Graphics::RHI::ResourceState::ShaderResource );
                                     data.inGlobalBufferHandles.material = pb.read( data.inGlobalBufferHandles.material, Graphics::RHI::ResourceState::ShaderResource );
                                    //Indirect
                                    if(data.useGPUCulling){
                                     data.inIndirectBufferHandle    = pb.read( data.inIndirectBufferHandle, Graphics::RHI::ResourceState::IndirectArgument );
                                     data.inCulledRedirectBufferHandle = pb.read( data.inCulledRedirectBufferHandle, Graphics::RHI::ResourceState::ShaderResource );} },

                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd = ctx.cmd;

        // RenderTargets
        auto* dsv = ctx.getTexture( data.outDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( data.matLayoutHandle );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea             = dsv->getDescription().size.to2D();
        info.depthStencilAttachment = { .texture = dsv };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------

        auto* set0 = data.persistentDescriptorSet;
        cmd->bindDescriptorSet( 0, set0, matLayout );

        // SPACE 1: Volatile Data (Views into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3), Redirection (t4)
        set1->attachBufferView( 0, data.inFrameView, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachBufferView( 1, data.inMeshesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 2, data.inMaterialsView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 3, data.inInstancesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 4, data.inLightsView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 5, data.inEnvsView, Graphics::RHI::ResourceState::ShaderResource );

        if ( data.useGPUCulling )
        {
            auto* culledBuf = ctx.getBuffer( data.inCulledRedirectBufferHandle );

            Graphics::RHI::BufferView culledView;
            culledView.buffer = culledBuf;
            culledView.offset = 0;
            culledView.size   = data.inRedirectionView.size;
            culledView.stride = data.inRedirectionView.stride;
            set1->attachBufferView( 6, culledView, Graphics::RHI::ResourceState::ShaderResource );
        } else
            set1->attachBufferView( 6, data.inRedirectionView, Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

        auto* ib = ctx.getBuffer( data.inGlobalBufferHandles.index );
        cmd->bindIndexBuffer( ib );
        auto* indirectBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferView.buffer;

        Graphics::PipelineHandle currentPsoHandle;
        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            // auto topologyType = (MaterialTopologyType)toGFXTopology( ()batch.topologyID );

            Graphics::PipelineHandle psoHandle = matArchetypes[batch.archetypeID].getPipeline(
                MaterialPassType::Depth, TopologyType::Triangles );

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