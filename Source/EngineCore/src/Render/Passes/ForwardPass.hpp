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
        Graphics::RGResourceHandle vertex;
        Graphics::RGResourceHandle index;
        Graphics::RGResourceHandle material;
        Graphics::RGResourceHandle volatileUBO;
    };

    struct Config {
        GlobalBufferHandles        bufferHandles;
        GPUScene::TransientOffsets uboOffsets;
        GPUScene*                  gpuScene;

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
                data.bufferHandles.index  = pb.read( data.bufferHandles.index,  Graphics::RHI::ResourceState::GeneralRead ); },

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
        auto* vb  = ctx.getBuffer( data.bufferHandles.vertex );
        auto* ib  = ctx.getBuffer( data.bufferHandles.index );
        auto* ubo = ctx.getBuffer( data.bufferHandles.volatileUBO );

        // SPACE 0: Persistent Data
        auto* set0 = ctx.allocateSet( matLayout, 0 );                        // Space 0
        set0->attach( 0, vb, Graphics::RHI::ResourceState::ShaderResource ); // t0
        set0->attach( 1, ib, Graphics::RHI::ResourceState::ShaderResource ); // t1

        cmd->bindDescriptorSet( 0, set0, matLayout );

        // SPACE 1: Volatile Data (Views into the giant UBO)
        auto* set1 = ctx.allocateSet( matLayout, 1 ); // Space 1

        // Frame (b0), Meshes (t0), Instances (t1), Lights (t2)
        set1->attachDynamic( 0, ubo, data.uboOffsets.frameOffset, sizeof( GPUFrame ), 0, Graphics::RHI::ResourceState::ConstantBuffer );
        set1->attachDynamic( 1, ubo, data.uboOffsets.meshOffset, scene.meshes().size() * sizeof( GPUMesh ), sizeof( GPUMesh ), Graphics::RHI::ResourceState::ShaderResource );
        set1->attachDynamic( 2, ubo, data.uboOffsets.instanceOffset, scene.instances().size() * sizeof( GPUInstance ), sizeof( GPUInstance ), Graphics::RHI::ResourceState::ShaderResource );
        set1->attachDynamic( 3, ubo, data.uboOffsets.lightOffset, scene.lights().size() * sizeof( GPULight ), sizeof( GPULight ), Graphics::RHI::ResourceState::ShaderResource );

        cmd->bindDescriptorSet( 1, set1, matLayout );

        // -----------------------------------------------------
        // 3. DRAW LOOP
        // -----------------------------------------------------
        const auto& instances     = scene.instances();
        const auto& matArchetypes = data.matLib->getArchetypesRaw();

        // TODO: Aquí iría el sorting de drawList en el futuro

        Graphics::PipelineHandle lastPipelineHandle;

        // uint                     lastMeshID = MAX_UINT32;
        for ( ulong i = 0; i < instances.size(); ++i )
        {
            const auto& inst = instances[i];
            if ( inst.active == 0 )
                continue;

            const auto& meshData = scene.meshes()[inst.meshID];

            Graphics::PipelineHandle targetPipelineHandle = matArchetypes[0].getPipeline( MaterialPassType::Opaque, MaterialTopologyType::Triangles );
            if ( targetPipelineHandle != lastPipelineHandle )
            {
                if ( targetPipelineHandle.isValid() )
                {
                    auto* pso = ctx.pipelines.getGraphicPipeline( targetPipelineHandle );
                    cmd->bindGraphicPipeline( pso );
                    lastPipelineHandle = targetPipelineHandle;

                    // NOTA IMPORTANTE:
                    // Al cambiar el PSO, DX12 *mantiene* los DescriptorSets 0 y 1 bindeados
                    // porque el RootSignature es el mismo (gracias al GlobalLayout).
                }
            }

            struct Push {
                uint instanceID;
            } push = { (uint)i };

            // Set instance ID
            cmd->pushConstants( 2, push );

            cmd->draw( meshData.indexCount, 1 );
        }

        cmd->endRendering();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END