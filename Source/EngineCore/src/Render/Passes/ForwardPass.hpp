#pragma once
#include "../GPUScene.h"
#include "../MaterialSystem.h"
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class ForwardPass : public IRenderPass
{
public:
    struct GlobalBufferHandles {
        Graphics::RGResourceHandle vertexBuffer;
        Graphics::RGResourceHandle indexBuffer;
        Graphics::RGResourceHandle mtrlBuffer;
        Graphics::RGResourceHandle volatileUBO;
    };

    struct Config {
        GlobalBufferHandles        res;
        GPUScene::TransientOffsets uboOffsets;
        GPUScene*                  scene;
        MaterialLibrary*           matLib;

        Graphics::RGResourceHandle outputColor;
        Graphics::RGResourceHandle outputDepth;
    };

    void registerShaders( Graphics::IShaderRegistry& ) override { /* NO OP */ }
    void createPipelines( Graphics::IPipelineRegistry& ) override { /* NO OP */ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {

        builder.addPass<Config>( "ForwardPass", seedData,

                                 // SETUP: Declaramos lecturas y escrituras
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                
                data.outputColor = pb.write( data.outputColor, Graphics::RHI::ResourceState::RenderTarget ); 
                data.outputDepth = pb.write( data.outputDepth, Graphics::RHI::ResourceState::DepthWrite );

                data.res.vertexBuffer = pb.read( data.res.vertexBuffer, Graphics::RHI::ResourceState::GeneralRead );
                data.res.indexBuffer  = pb.read( data.res.indexBuffer,  Graphics::RHI::ResourceState::GeneralRead ); },

                                 // EXECUTE
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto*       cmd   = ctx.cmd;
        const auto& scene = *data.scene;

        // RenderTargets
        auto* rtv = ctx.getTexture( data.outputColor );
        auto* dsv = ctx.getTexture( data.outputDepth );

        // Begin Rendering
        Graphics::RHI::RenderingDesc info;
        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = targetTex } );
        info.depthStencilAttachment = { .texture = depthTex };
        ctx.cmd->beginRendering( info );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------
        auto* vb  = ctx.getBuffer( data.res.vertexBuffer );
        auto* ib  = ctx.getBuffer( data.res.indexBuffer );
        auto* ubo = ctx.getBuffer( data.res.volatileUBO );

        // SPACE 1: Persistent Data
        auto* set0 = ctx.allocateSet( dummyPipe->getDescription().layout, 0 ); // Space 0
        set0->attach( 0, vb, Graphics::RHI::ResourceState::ShaderResource );   // t0
        set0->attach( 1, ib, Graphics::RHI::ResourceState::ShaderResource );   // t1
        cmd->bindDescriptorSet( 0, set0 );

        // SPACE 1: Volatile Data (Views into the giant UBO)
        auto* set1 = ctx.allocateSet( dummyPipe->getDescription().layout, 1 ); // Space 1

        // View 1: Frame Data (CBV) - b0
        set1->attach( 0, ubo, data.offsets.frameOffset, sizeof( GPUFrame ) );

        // View 2: Meshes (Structured) - t0
        // Nota: Stride = sizeof(GPUMesh), Size = Resto del buffer o exacto
        set1->attach( 0, ubo, data.offsets.meshOffset, scene.meshes().size() * sizeof( GPUMesh ), sizeof( GPUMesh ) );

        // View 3: Instances (Structured) - t1
        set1->attach( 1, ubo, data.offsets.instanceOffset, scene.instances().size() * sizeof( GPUInstance ), sizeof( GPUInstance ) );

        // View 4: Lights (Structured) - t2
        set1->attach( 2, ubo, data.offsets.lightOffset, scene.lights().size() * sizeof( GPULight ), sizeof( GPULight ) );

        cmd->bindDescriptorSet( 1, set1 );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& instances     = scene.instances();
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

        // Optimización: Agrupar por Material y Mesh para reducir cambios de estado.
        // for(){
        //     agrupar por material y mesh, reordenar array (Primero todas con material x, etc)
        // }

        Graphics::PipelineHandle lastPipeline;
        uint                     lastMeshID = MAX_UINT32;
        for ( ulong i = 0; i < instances.size(); ++i )
        {
            const auto& inst = instances[i];
            if ( inst.active == 0 )
                continue;

            const auto&              meshData = scene.meshes()[inst.meshID];
            Graphics::PipelineHandle pipeline;
            if ( lastPipeline != pipeline )
                // pipeline = matArchetypes[material.archeTypeID].pipeline[Opaque][meshData.topology]
                pipeline = matArchetypes[0].pipeline[MaterialPassType::Opaque][MaterialTopologyType::Triangles];

            if ( pipeline.isValid() )
            {
                auto* pso = ctx.pipelines.getGraphicPipeline( pipeline );
                cmd->bindGraphicPipeline( pso );
            }

            struct Push {
                uint instanceID;
            } push = { (uint)i };

            // Set instance ID
            cmd->pushConstants( 2, push );

            cmd->draw( meshData.indexCount, 1 );
        }

        cmd->endRenderPass();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END