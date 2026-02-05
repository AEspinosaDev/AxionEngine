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
        GlobalBufferHandles inGlobalBufferHandles;

        Graphics::RHI::BufferView inFrameView;
        Graphics::RHI::BufferView inMeshesView;
        Graphics::RHI::BufferView inMaterialsView;
        Graphics::RHI::BufferView inInstancesView;
        Graphics::RHI::BufferView inLightsView;
        Graphics::RHI::BufferView inRedirectionView;

        IndirectCommandData        indirectData;
        Graphics::RGResourceHandle inIndirectBufferHandle;
        Graphics::RGResourceHandle inCulledRedirectBufferHandle;
        bool                       useGPUCulling = false;

        GPUScene*                      gpuScene;
        MaterialLibrary*               matLib;
        Graphics::PipelineLayoutHandle matLayoutHandle;

        Graphics::RGResourceHandle outColorHandle;
        Graphics::RGResourceHandle outDepthHandle;
        bool                       clearDepth = false;
    };

    void registerShaders( Graphics::IShaderRegistry& ) override { /* NO OP */ }
    void createPipelines( Graphics::IPipelineRegistry& ) override { /* NO OP */ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {

        builder.addPass<Config>( "ForwardPass", seedData,

                                 // SETUP: Declaramos lecturas y escrituras
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                                    //Rendertargets
                                     data.outColorHandle = pb.write( data.outColorHandle, Graphics::RHI::ResourceState::RenderTarget );
                                     data.outDepthHandle = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::DepthWrite );
                                    //Global
                                     data.inGlobalBufferHandles.vertex   = pb.read( data.inGlobalBufferHandles.vertex, Graphics::RHI::ResourceState::ShaderResource );
                                     data.inGlobalBufferHandles.index    = pb.read( data.inGlobalBufferHandles.index, Graphics::RHI::ResourceState::ShaderResource );
                                     data.inGlobalBufferHandles.material = pb.read( data.inGlobalBufferHandles.material, Graphics::RHI::ResourceState::ShaderResource );
                                    //Indirect
                                    if(data.useGPUCulling){
                                     data.inIndirectBufferHandle    = pb.read( data.inIndirectBufferHandle, Graphics::RHI::ResourceState::IndirectArgument );
                                     data.inCulledRedirectBufferHandle = pb.read( data.inCulledRedirectBufferHandle, Graphics::RHI::ResourceState::ShaderResource );} },

                                 // EXECUTE
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd   = ctx.cmd;
        auto& scene = *data.gpuScene;

        // RenderTargets
        auto* rtv = ctx.getTexture( data.outColorHandle );
        auto* dsv = ctx.getTexture( data.outDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( data.matLayoutHandle );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea = rtv->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = rtv } );
        info.depthStencilAttachment = { .texture = dsv, .loadOp = !data.clearDepth ? Graphics::RHI::LoadOp::Load : Graphics::RHI::LoadOp::Clear };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------
        auto* vb   = ctx.getBuffer( data.inGlobalBufferHandles.vertex );
        auto* ib   = ctx.getBuffer( data.inGlobalBufferHandles.index );
        auto* mtlb = ctx.getBuffer( data.inGlobalBufferHandles.material );

        // SPACE 0: Persistent Data
        auto* set0 = ctx.allocateSet( matLayout, 0 );                          // Space 0
        set0->attach( 0, vb, Graphics::RHI::ResourceState::ShaderResource );   // t0
        set0->attach( 1, ib, Graphics::RHI::ResourceState::ShaderResource );   // t1
        set0->attach( 2, mtlb, Graphics::RHI::ResourceState::ShaderResource ); // t2

        cmd->bindDescriptorSet( 0, set0, matLayout );

        // SPACE 1: Volatile Data (Views into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3), Redirection (t4)
        set1->attachBufferView( 0, data.inFrameView, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachBufferView( 1, data.inMeshesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 2, data.inMaterialsView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 3, data.inInstancesView, Graphics::RHI::ResourceState::ShaderResource );
        set1->attachBufferView( 4, data.inLightsView, Graphics::RHI::ResourceState::ShaderResource );

        if ( data.useGPUCulling )
        {
            auto* culledBuf = ctx.getBuffer( data.inCulledRedirectBufferHandle );

            Graphics::RHI::BufferView culledView;
            culledView.buffer = culledBuf;
            culledView.offset = 0;
            culledView.size   = data.inRedirectionView.size;
            culledView.stride = data.inRedirectionView.stride;
            set1->attachBufferView( 5, culledView, Graphics::RHI::ResourceState::ShaderResource );
        } else
            set1->attachBufferView( 5, data.inRedirectionView, Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

#if DRAW_INDIRECT
        cmd->bindIndexBuffer( ib );
        auto* indirectBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferView.buffer;

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
                    indirectBuffer,
                    batch.bufferOffset,
                    batch.drawCount );
            }
        }
#else

        const auto& sortedKeys = scene.getSortedKeys();
        const auto& instances  = scene.instances();
        const auto& meshes     = scene.meshes();

        uint                     lastArchetypeID = UINT32_MAX;
        uint                     lastTopologyID  = UINT32_MAX;
        Graphics::PipelineHandle lastPipelineHandle;

        for ( const auto& keyData : sortedKeys )
        {
            uint        originalIdx = keyData.originalInstanceIdx;
            const auto& inst        = instances[originalIdx];

            if ( inst.active == 0 )
                continue;

            const auto& mesh = meshes[inst.meshID];
            uint        currentArch, currentTopo, currentMeshID;
            keyData.unpack( currentArch, currentTopo, currentMeshID );

            if ( currentArch != lastArchetypeID || currentTopo != lastTopologyID )
            {
                Graphics::PipelineHandle targetHandle = matArchetypes[currentArch].getPipeline(
                    MaterialPassType::Opaque, TopologyType::Triangles );

                if ( targetHandle.isValid() && targetHandle != lastPipelineHandle )
                {
                    auto* pso = ctx.pipelines.getGraphicPipeline( targetHandle );
                    if ( pso )
                    {
                        cmd->bindGraphicPipeline( pso );
                        lastPipelineHandle = targetHandle;
                    }
                }
                lastArchetypeID = currentArch;
                lastTopologyID  = currentTopo;
            }

            struct Push {
                uint instanceID;
            };
            Push p = { originalIdx };
            cmd->pushConstants( 2, p ); // Space 2

            cmd->drawIndexed(
                mesh.indexCount,
                1,
                mesh.indexOffset / 4,                                  // Asumiendo indices de 32 bits (4 bytes)
                (int)( mesh.vertexOffset / sizeof( Assets::Vertex ) ), // Vertex Offset en cantidad de vértices, no bytes
                0 );
        }

#endif
        cmd->endRendering();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END