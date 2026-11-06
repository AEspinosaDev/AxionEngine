#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>
#include <algorithm>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class VisResolvePass : public IRenderPass
{
public:
    struct Config {
        IndirectCommandPayload indirectData;

        // Global buffers (bindless): geometry pages for the triangle, materials payloads
        Rasterizer::Resources::GlobalBufferHandles inGlobalBufferHandles;

        STLW::Vector<Graphics::RGResourceHandle> inParticleBufferHandles; ///< Data + visible list of every system. Read through the bindless heap: invisible to the graph

        Graphics::RGResourceHandle inVisHandle;
        Graphics::RGResourceHandle outColorHandle;

        u32 workgroupSize = 8;

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr;

        u32               materialPassSlot;
        IMaterialLibrary* matLib;
        u32               ioSetId;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Config&                 config ) {

        builder.addPass<Config>( "Vis Resolve Pass", config, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.inVisHandle  = pb.read( data.inVisHandle, Graphics::RHI::ResourceState::ShaderResource  );
                data.inGlobalBufferHandles.read( pb );
                for ( auto& handle : data.inParticleBufferHandles )
                    handle = pb.read( handle, Graphics::RHI::ResourceState::ShaderResource );
                data.outColorHandle = pb.write( data.outColorHandle ); }, [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        Graphics::RHI::ICommandList* cmd = ctx.cmd;

        // RenderMaterial
        const MaterialPassProfile& matPassProfile = data.matLib->getPassProfile( data.materialPassSlot );

        // Global Layout
        auto* matLayout = ctx.pipelines.getLayout( matPassProfile.layoutHandle );

        // IO Tex
        auto* texIn  = ctx.getTexture( data.inVisHandle );
        auto* texOut = ctx.getTexture( data.outColorHandle );

        // -----------------------------------------------------
        // BINDING GLOBAL RESOURCES (Space 0: frame data. Geometry & materials are bindless)
        // -----------------------------------------------------

        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Volatile Data
        auto* ioSet = ctx.allocateSet( matLayout, data.ioSetId );

        ioSet->attach( Rasterizer::Config::REG_PASS_SRV_IMAGE_BASE, Graphics::RHI::DescriptorType::SRV_Image, texIn );  // gVisBuffer (PASS_SRV_IMAGE_0)
        ioSet->attach( Rasterizer::Config::REG_PASS_UAV_IMAGE_BASE, Graphics::RHI::DescriptorType::UAV_Image, texOut ); // gOutColor  (PASS_UAV_IMAGE_0)

        cmd->bindDescriptorSet( data.ioSetId, ioSet, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        Graphics::PipelineHandle currentPsoHandle;
        auto                     resolveBundle = [&]( u32 psoBundleID ) {
            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( psoBundleID, data.materialPassSlot );
            if ( !psoHandle.isValid() )
                return;

            if ( currentPsoHandle != psoHandle )
            {
                auto* pso = ctx.pipelines.getComputePipeline( psoHandle );
                cmd->bindComputePipeline( pso );
                currentPsoHandle = psoHandle;
            }

            // Compute PSO is shared by every bundle of the archetype: the bundle filter goes in the push constant
            cmd->pushConstants( psoBundleID );

            auto size = texIn->getDescription().size;
            u32  wgs  = data.workgroupSize;
            cmd->dispatch( { ( size.width + wgs - 1 ) / wgs, ( size.height + wgs - 1 ) / wgs, 1 } );
        };

        const auto& batches = data.indirectData.batches;
        for ( const auto& batch : batches )
        {
            if ( batch.drawCount != 0 )
                resolveBundle( batch.psoBundleID );
        }

        // Bundles only used by particles: the rest were resolved above
        for ( const auto& particleBatch : data.indirectData.particleBatches )
        {
            const bool resolved = std::any_of( batches.begin(), batches.end(), [&]( const IndirectDrawBatch& batch ) { return batch.psoBundleID == particleBatch.psoBundleID; } );
            if ( !resolved )
                resolveBundle( particleBatch.psoBundleID );
        }
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END