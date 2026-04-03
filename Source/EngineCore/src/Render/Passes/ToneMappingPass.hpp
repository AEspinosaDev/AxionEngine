#pragma once
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class ToneMappingPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inputHandle;  // HDR
        Graphics::RGResourceHandle outputHandle; // LDR
        u32                       tonemapType;
        float                      exposure = 1.0f;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "Tonemapping Shader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Postpro/Tonemapping.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "computeMain" )
                        .load();
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "Tonemapping Pipeline" )
                         .shader( _shHandle )
                         .create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Config&                 config ) {

        builder.addPass<Config>( "TonemappingPass", config, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.inputHandle  = pb.read( data.inputHandle ); 
                data.outputHandle = pb.write( data.outputHandle ); }, [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* texIn  = ctx.getTexture( data.inputHandle );
        auto* texOut = ctx.getTexture( data.outputHandle );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->pushConstants( 1, data.exposure );

        auto size = texIn->getDescription().size;
        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END