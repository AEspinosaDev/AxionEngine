#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Temporal anti-aliasing resolve (HDR, before the tonemapping). Its output is the history of the next frame
class TAAPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inputCurrent;  ///< HDR, jittered
        Graphics::RGResourceHandle inputHistory;  ///< HDR, TAA output of the previous frame
        Graphics::RGResourceHandle inputVelocity; ///< UV units: current - previous
        Graphics::RGResourceHandle outputHandle;  ///< HDR

        Graphics::SamplerHandle linearSamplerHandle; ///< Linear + clamp (history resampling)

        float exposure     = 1.0f;  ///< Same factor as the tonemapper: the blending is done in display range
        bool  resetHistory = false; ///< First frame, resize or camera cut: output = current frame
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {

        _shHandle = shaders.shader( "TAA Shader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Postpro/TAA.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "computeMain" )
                        .autoReflect( false ) // Global layout
                        .load();
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "TAA Pipeline" )
                         .shader( _shHandle )
                         .setLayout( _globalLayout )
                         .create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Config&                 config ) {

        builder.addPass<Config>( "TAAPass", config, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.inputCurrent  = pb.read( data.inputCurrent );
                data.inputHistory  = pb.read( data.inputHistory );
                data.inputVelocity = pb.read( data.inputVelocity );
                data.outputHandle  = pb.write( data.outputHandle ); }, [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* texCurr = ctx.getTexture( data.inputCurrent );
        auto* texPrev = ctx.getTexture( data.inputHistory );
        auto* texVel  = ctx.getTexture( data.inputVelocity );
        auto* texOut  = ctx.getTexture( data.outputHandle );

        auto* sampler = ctx.resources.getSampler( data.linearSamplerHandle );

        auto* ioSet = ctx.allocateSet( pso->getDescription().layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::texture( 0 ), Graphics::RHI::DescriptorType::SRV_Image, texCurr );
        ioSet->attach( Graphics::PassIO::texture( 1 ), Graphics::RHI::DescriptorType::SRV_Image, texPrev );
        ioSet->attach( Graphics::PassIO::texture( 2 ), Graphics::RHI::DescriptorType::SRV_Image, texVel );
        ioSet->attach( Graphics::PassIO::rwTexture( 0 ), Graphics::RHI::DescriptorType::UAV_Image, texOut );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet );

        const auto size = texCurr->getDescription().size;

        /// @brief Mirrors TAA.slang PushConstant (HLSL packing: float2 at 0, then 4 B scalars)
        struct TAAConstants {
            Math::Vec2 inverseTexSize;
            u32        samplerID; ///< Bindless linear clamp sampler
            float      exposure;
            u32        resetHistory;
        } constants { Math::Vec2( 1.0f / size.width, 1.0f / size.height ),
                      sampler->getBindlessIdx(),
                      data.exposure,
                      data.resetHistory ? 1u : 0u };
        ctx.cmd->pushConstants( constants );

        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
