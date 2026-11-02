#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class VisResolvePass : public IRenderPass
{
public:
    struct Config {
        IndirectCommandPayload indirectData;

        // Global buffers read by the resolve (vertices / indices for the triangle, materials payloads)
        Graphics::RGResourceHandle inVertexHandle;
        Graphics::RGResourceHandle inIndexHandle;
        Graphics::RGResourceHandle inMaterialsHandle;

        Graphics::RGResourceHandle inVisHandle;
        Graphics::RGResourceHandle outColorHandle;

        u32 workgroupSize = 8;

        Graphics::RHI::IDescriptorSet* persistentDescriptorSet = nullptr;
        Graphics::RHI::IDescriptorSet* transientDescriptorSet  = nullptr;

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
                data.inVertexHandle    = pb.read( data.inVertexHandle, Graphics::RHI::ResourceState::ShaderResource );
                data.inIndexHandle     = pb.read( data.inIndexHandle, Graphics::RHI::ResourceState::ShaderResource );
                data.inMaterialsHandle = pb.read( data.inMaterialsHandle, Graphics::RHI::ResourceState::ShaderResource );
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
        // BINDING GLOBAL RESOURCES (Space 0 & 1)
        // -----------------------------------------------------

        auto* set0 = data.persistentDescriptorSet;
        cmd->bindDescriptorSet( 0, set0, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        auto* set1 = data.transientDescriptorSet;
        cmd->bindDescriptorSet( 1, set1, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 2: Volatile Data
        auto* set2 = ctx.allocateSet( matLayout, data.ioSetId );

        set2->attach( Rasterizer::Config::REG_PASS_SRV_IMAGE_BASE, Graphics::RHI::DescriptorType::SRV_Image, texIn );  // gVisBuffer (PASS_SRV_IMAGE_0)
        set2->attach( Rasterizer::Config::REG_PASS_UAV_IMAGE_BASE, Graphics::RHI::DescriptorType::UAV_Image, texOut ); // gOutColor  (PASS_UAV_IMAGE_0)

        cmd->bindDescriptorSet( 2, set2, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        Graphics::PipelineHandle currentPsoHandle;
        for ( const auto& batch : data.indirectData.batches )
        {
            if ( batch.drawCount == 0 )
                continue;

            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( batch.bundleID, data.materialPassSlot );
            if ( !psoHandle.isValid() )
                continue;

            if ( currentPsoHandle != psoHandle )
            {
                auto* pso = ctx.pipelines.getComputePipeline( psoHandle );
                cmd->bindComputePipeline( pso );
                currentPsoHandle = psoHandle;
            }

            // Compute PSO is shared by every bundle of the archetype: the bundle filter goes in the push constant
            cmd->pushConstants( batch.bundleID );

            auto size = texIn->getDescription().size;
            u32  wgs  = data.workgroupSize;
            cmd->dispatch( { ( size.width + wgs - 1 ) / wgs, ( size.height + wgs - 1 ) / wgs, 1 } );
        }
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END