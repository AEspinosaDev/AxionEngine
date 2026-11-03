#pragma once
#include <Axion/Graphics/Subsystems/IRenderGraph.h>
#include <Render/DrawIndirect.h>
#include <Render/MaterialLibrary.h>
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class VisPass : public IRenderPass
{
public:
    struct Config {
        IndirectCommandPayload indirectData;

        // In Buffers
        Rasterizer::Resources::GlobalBufferHandles inGlobalBufferHandles; ///< Geometry pages + materials (bindless, declared for the transitions)
        Graphics::RGResourceHandle                 inIndirectBufferHandle;
        Graphics::RGResourceHandle                 inCulledRedirectBufferHandle;
        Graphics::BufferSlice                      inInstanceListSlice; ///< Full sorted instance list (GPU culling off)
        bool                                       useGPUCulling = false;

        // Out RTOs
        Graphics::RGResourceHandle outVisHandle;
        Graphics::RGResourceHandle outVelocityHandle;
        Graphics::RGResourceHandle outDepthHandle;

        // Resources
        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr;

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
                                    data.inGlobalBufferHandles.read( pb );
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
        // BINDING GLOBAL RESOURCES (Space 0: frame data. Geometry & materials are bindless)
        // -----------------------------------------------------

        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, matLayout );

        // SPACE 1: instances to draw (culled list or the full sorted one)
        auto* ioSet = ctx.allocateSet( matLayout, Graphics::PassIO::SET_INDEX );
        if ( data.useGPUCulling )
            ioSet->attach( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inCulledRedirectBufferHandle ) );
        else
            ioSet->attachBufferSlice( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, data.inInstanceListSlice );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, matLayout );

        // -----------------------------------------------------
        // 3. DISPATCH LOOP
        // -----------------------------------------------------

        // Commands: GPU copy filled by the culling, or the CPU upload slice (instance counts already written)
        auto* argBuffer = data.useGPUCulling ? ctx.getBuffer( data.inIndirectBufferHandle ) : data.indirectData.commandBufferSlice.container;
        u64   argOffset = data.useGPUCulling ? 0 : data.indirectData.commandBufferSlice.offset;

        // Cache PSO
        Graphics::PipelineHandle currentPsoHandle;
        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( batch.psoBundleID, data.materialPassSlot );
            if ( !psoHandle.isValid() )
                continue;

            if ( currentPsoHandle != psoHandle )
            {
                auto* pso = ctx.pipelines.getMeshPipeline( psoHandle );
                cmd->bindMeshPipeline( pso );
                currentPsoHandle = psoHandle;
            }

            // Meshlet cone culling only when the PSO culls back faces (CullMode::Front keeps the outer faces in the engine convention)
            const u32 meshletConeCulling = data.matLib->getBundleRenderState( batch.psoBundleID ).cullMode == Graphics::CullMode::Front ? 1u : 0u;
            cmd->pushConstants( meshletConeCulling, 1 ); // Offset 1: baseInstanceID (offset 0) is written by the command signature

            // One mesh shading dispatch per command: x = meshlet chunk (amplification), y = instance
            cmd->dispatchMeshIndirect( argBuffer, argOffset + batch.bufferOffset, batch.drawCount );
        }

        cmd->endRendering();
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END