#pragma once
#include "../DrawIndirect.h"
#include "../GPUScene.h"
#include "../MaterialSystem.h"
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN

#define DRAW_INDIRECT 1

namespace Core::Render {

class ForwardPass : public IRenderPass
{
public:
    struct GlobalBufferHandles {
        Graphics::RGResourceHandle vertex;
        Graphics::RGResourceHandle index;
        Graphics::RGResourceHandle material;
    };

    struct Config {
        GlobalBufferHandles bufferHandles;

        Graphics::RHI::BufferView frameView;
        Graphics::RHI::BufferView meshesView;
        Graphics::RHI::BufferView materialsView;
        Graphics::RHI::BufferView instancesView;
        Graphics::RHI::BufferView lightsView;

        IndirectCommandData indirectData;

        GPUScene*                      gpuScene;
        MaterialLibrary*               matLib;
        Graphics::PipelineLayoutHandle matLayoutHandle;

        Graphics::RGResourceHandle outputColorHandle;
        Graphics::RGResourceHandle outputDepthHandle;
    };

    void registerShaders( Graphics::IShaderRegistry& ) override { /* NO OP */ }
    void createPipelines( Graphics::IPipelineRegistry& ) override { /* NO OP */ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {

        builder.addPass<Config>( "ForwardPass", seedData,

                                 // SETUP: Declaramos lecturas y escrituras
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                
                data.outputColorHandle = pb.write( data.outputColorHandle, Graphics::RHI::ResourceState::RenderTarget ); 
                data.outputDepthHandle = pb.write( data.outputDepthHandle, Graphics::RHI::ResourceState::DepthWrite );

                data.bufferHandles.vertex = pb.read( data.bufferHandles.vertex, Graphics::RHI::ResourceState::GeneralRead );
                data.bufferHandles.index = pb.read( data.bufferHandles.index, Graphics::RHI::ResourceState::GeneralRead );
                data.bufferHandles.material  = pb.read( data.bufferHandles.material,  Graphics::RHI::ResourceState::GeneralRead ); },

                                 // EXECUTE
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd   = ctx.cmd;
        auto& scene = *data.gpuScene;

        // RenderTargets
        auto* rtv = ctx.getTexture( data.outputColorHandle );
        auto* dsv = ctx.getTexture( data.outputDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( data.matLayoutHandle );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea = rtv->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = rtv } );
        info.depthStencilAttachment = { .texture = dsv };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------
        auto* vb   = ctx.getBuffer( data.bufferHandles.vertex );
        auto* ib   = ctx.getBuffer( data.bufferHandles.index );
        auto* mtlb = ctx.getBuffer( data.bufferHandles.material );

        // SPACE 0: Persistent Data
        auto* set0 = ctx.allocateSet( matLayout, 0 );                          // Space 0
        set0->attach( 0, vb, Graphics::RHI::ResourceState::ShaderResource );   // t0
        set0->attach( 1, ib, Graphics::RHI::ResourceState::ShaderResource );   // t1
        set0->attach( 2, mtlb, Graphics::RHI::ResourceState::ShaderResource ); // t2

        cmd->bindDescriptorSet( 0, set0, matLayout );

        // SPACE 1: Volatile Data (Views into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3)
        set1->attachBufferView( 0, data.frameView, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachBufferView( 1, data.meshesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 2, data.materialsView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 3, data.instancesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 4, data.lightsView, Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

#if DRAW_INDIRECT
        cmd->bindIndexBuffer( ib );

        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            // auto topologyType = (MaterialTopologyType)toGFXTopology( ()batch.topologyID );

            Graphics::PipelineHandle psoHandle = matArchetypes[batch.archetypeID].getPipeline(
                MaterialPassType::Opaque, TopologyType::Triangles );

            if ( psoHandle.isValid() )
            {
                auto* pso = ctx.pipelines.getGraphicPipeline( psoHandle );
                cmd->bindGraphicPipeline( pso );

                cmd->drawIndexedIndirect(
                    data.indirectData.bufferView.buffer,
                    batch.bufferOffset,
                    batch.drawCount );
            }
        }
#else

        const auto& sortedKeys = scene.getSortedKeys();
        const auto& instances  = scene.instances();
        const auto& meshes     = scene.meshes();

        uint lastArchetypeID = UINT32_MAX;
        uint lastTopologyID  = UINT32_MAX;

        Graphics::PipelineHandle lastPipelineHandle;

        for ( const auto& keyData : sortedKeys )
        {
            uint        originalIdx = keyData.originalInstanceIdx;
            const auto& inst        = instances[originalIdx];

            if ( inst.active == 0 )
                continue;

            const auto& mesh = meshes[inst.meshID];

            uint currentArch, currentTopo, currentMatID;
            keyData.unpack( currentArch, currentTopo, currentMatID );

            if ( currentArch != lastArchetypeID || currentTopo != lastTopologyID )
            {
                // auto topologyType = (MaterialTopologyType)currentTopo;

                Graphics::PipelineHandle targetHandle = matArchetypes[currentArch].getPipeline(
                    MaterialPassType::Opaque,
                    TopologyType::Triangles );

                if ( targetHandle.isValid() && targetHandle != lastPipelineHandle )
                {
                    auto* pso = ctx.pipelines.getGraphicPipeline( targetHandle );
                    cmd->bindGraphicPipeline( pso );
                    lastPipelineHandle = targetHandle;
                }

                lastArchetypeID = currentArch;
                lastTopologyID  = currentTopo;
            }

            struct Push {
                uint instanceID;
            } push = { originalIdx };

            cmd->pushConstants( 2, push );

            cmd->draw( mesh.indexCount, 1 );
        }

#endif
        cmd->endRendering();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END