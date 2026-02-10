#pragma once
#include "Axion/Graphics/Passes/Recipe.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::Passes {

#pragma region ToneMapping

struct ToneMappingData {
    RGResourceHandle inputHDR;
    RGResourceHandle outputLDR;
    float            exposure = 1.0f;

    struct GPUPayload {
        float exposure;
        float gamma;
        float type; // ToneMappingType
    } payload;
};

struct ToneMapping : public IPassRecipe<ToneMappingData> {

    PipelineHandle pipelineHandle;

    RGResourceHandle inputHandle;
    RGResourceHandle outputHandle;

    void init( IRenderer& rnd ) override {
        // Shaders
        auto sh = rnd.shaders()
                      .shader( "TmShader" )
                      .asDXIL()
                      .path( AXION_SHADER_DIR "/Slang/Postpro/Tonemapping.slang" )
                      .include( AXION_SHADER_DIR "/Slang/Common" )
                      .cs( "computeMain" )
                      .load();
        rnd.shaders().compileShader( sh );
        // Pipeline
        pipelineHandle = rnd.pipelines().compute( "TmPipeline" ).shader( "TmShader" ).create();
    }

    void setup( RenderPassBuilder& builder, ToneMappingData& data ) override {
        data.inputHDR  = builder.read( inputHandle );
        data.outputLDR = builder.write( outputHandle );
    }

    void execute( const ToneMappingData& data, RenderPassContext& ctx ) override {
        auto* pso = ctx.pipelines.getComputePipeline( pipelineHandle );

        auto* texIn  = ctx.getTexture( data.inputHDR );
        auto* texOut = ctx.getTexture( data.outputLDR );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->pushConstants( 1, data.exposure );

        // Dispatch (assuming 8x8 thread group)
        auto size = texIn->getDescription().size;
        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }
};

#pragma endregion
#pragma region FXAA

struct FXAAData {
    RGResourceHandle input;
    RGResourceHandle output;
};

struct FXAA : public IPassRecipe<FXAAData> {

    PipelineHandle pipelineHandle;

    void init( IRenderer& rnd ) override {
        // Shaders
        auto sh = rnd.shaders()
                      .shader( "FXAAShader" )
                      .asDXIL()
                      .path( AXION_SHADER_DIR "/Slang/Postpro/FXAA.slang" )
                      .include( AXION_SHADER_DIR "/Slang/Common" )
                      .cs( "computeMain" )
                      .load();
        rnd.shaders().compileShader( sh );
        // Pipeline
        pipelineHandle = rnd.pipelines().compute( "FXAAPipeline" ).shader( "FXAAShader" ).create();
    }

    void setup( RenderPassBuilder& builder, FXAAData& data ) override {
        data.input  = builder.read( data.input );
        data.output = builder.write( data.output );
    }

    void execute( const FXAAData& data, RenderPassContext& ctx ) override {
        auto* pso = ctx.pipelines.getComputePipeline( pipelineHandle );

        auto* texIn  = ctx.getTexture( data.input );
        auto* texOut = ctx.getTexture( data.output );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        // Dispatch (assuming 8x8 thread group)
        auto size = texIn->getDescription().size;
        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }
};

#pragma endregion

} // namespace Graphics::Passes

AXION_NAMESPACE_END
