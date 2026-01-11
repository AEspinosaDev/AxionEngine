#pragma once
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class UploadPass : public IRenderPass
{
public:
    struct Data {
        Graphics::RGResourceHandle inputHDR;
        Graphics::RGResourceHandle outputLDR;
        uint                       tonemapType;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
       
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
       
    }

    void setup( Graphics::RenderPassBuilder& builder, Data& data ) {
        data.inputHDR  = builder.read( data.inputHDR );
        data.outputLDR = builder.write( data.outputLDR );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* texIn  = ctx.getTexture( data.inputHDR );
        auto* texOut = ctx.getTexture( data.outputLDR );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        auto size = texIn->getDescription().size;
        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Data&                   data ) {
        builder.addPass<TonemappingPass>( "TonemappingPass", *this );
    }

private:
    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END