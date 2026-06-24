#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/DrawIndirect.h>
#include <Render/GPUScene.h>
#include <Render/MaterialLibrary.h>
#include <Render/PassManager.h>

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

        Graphics::BufferSlice inFrameSlice;
        Graphics::BufferSlice inMeshesSlice;
        Graphics::BufferSlice inMaterialsSlice;
        Graphics::BufferSlice inInstancesSlice;
        Graphics::BufferSlice inLightsSlice;
        Graphics::BufferSlice inEnvsSlice;
        Graphics::BufferSlice inRedirectionSlice;

        IndirectCommandPayload     indirectData;
        Graphics::RGResourceHandle inIndirectBufferHandle;
        Graphics::RGResourceHandle inCulledRedirectBufferHandle;
        bool                       useGPUCulling = false;

        Graphics::RHI::IDescriptorSet* persistentDescriptorSet = nullptr;

        GPUScene*         gpuScene;
        u32               materialPassSlot;
        IMaterialLibrary* matLib;

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

        // RenderMaterial
        const MaterialPassProfile& matPassProfile = data.matLib->getPassProfile( data.materialPassSlot );

        // RenderTargets
        auto* rtv = ctx.getTexture( data.outColorHandle );
        auto* dsv = ctx.getTexture( data.outDepthHandle );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( matPassProfile.layoutHandle );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea = rtv->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = rtv } );
        info.depthStencilAttachment = { .texture = dsv, .loadOp = !data.clearDepth ? Graphics::RHI::LoadOp::Load : Graphics::RHI::LoadOp::Clear };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------

        // Persistent Set
        auto* set0 = data.persistentDescriptorSet;
        cmd->bindDescriptorSet( 0, set0, matLayout );

        // SPACE 1: Volatile Data (Slices into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Materials (t1), Instances (t2, Lights (t3), Redirection (t4)
        set1->attachBufferSlice( 0, Graphics::RHI::DescriptorType::CBV, data.inFrameSlice );
        set1->attachBufferSlice( 1, Graphics::RHI::DescriptorType::SRV_Buffer, data.inMeshesSlice );
        set1->attachBufferSlice( 2, Graphics::RHI::DescriptorType::SRV_Buffer, data.inMaterialsSlice );
        set1->attachBufferSlice( 3, Graphics::RHI::DescriptorType::SRV_Buffer, data.inInstancesSlice );
        set1->attachBufferSlice( 4, Graphics::RHI::DescriptorType::SRV_Buffer, data.inLightsSlice );
        set1->attachBufferSlice( 5, Graphics::RHI::DescriptorType::SRV_Buffer, data.inEnvsSlice );

        if ( data.useGPUCulling )
        {
            auto* culledBuf = ctx.getBuffer( data.inCulledRedirectBufferHandle );

            Graphics::BufferSlice culledSlice;
            culledSlice.container = culledBuf;
            culledSlice.offset    = 0;
            culledSlice.size      = data.inRedirectionSlice.size;
            culledSlice.stride    = data.inRedirectionSlice.stride;
            set1->attachBufferSlice( 6, Graphics::RHI::DescriptorType::SRV_Buffer, culledSlice );
        } else
            set1->attachBufferSlice( 6, Graphics::RHI::DescriptorType::SRV_Buffer, data.inRedirectionSlice );

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------

#if DRAW_INDIRECT
        auto* ib = ctx.getBuffer( data.inGlobalBufferHandles.index );
        cmd->bindIndexBuffer( ib );
        auto* indirectBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferSlice.container;

        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( batch.psoID, data.materialPassSlot );

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