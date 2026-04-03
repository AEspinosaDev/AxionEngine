#pragma once
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class FXAAPass : public IRenderPass
{
public:
    enum class QualityPreset : byte
    {
        Low,
        Medium,
        High,
        Ultra
    };

    void setQuality( QualityPreset q ) { _quality = q; }

    struct Config {
        Graphics::RGResourceHandle inputHandle;  // LDR
        Graphics::RGResourceHandle outputHandle; // LDR

        // Choose the amount of sub-pixel aliasing removal.
        // This can effect sharpness.
        //   1.00 - upper limit (softer)
        //   0.75 - default amount of filtering
        //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
        //   0.00 - completely off
        float subpix = 0.75f;

        // The minimum amount of local contrast required to apply algorithm.
        //   0.333 - too little (faster)
        //   0.250 - low quality
        //   0.166 - default
        //   0.125 - high quality
        //   0.063 - overkill (slower)
        float edgeThreshold = 0.166f;

        // Trims the algorithm from processing darks.
        //   0.0833 - upper limit (default, the start of visible unfiltered edges)
        //   0.0625 - high quality (faster)
        //   0.0312 - visible limit (slower)
        float edgeThresholdMin = 0.0833f;

        Graphics::SamplerHandle linearSamplerHandle;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        Graphics::Shader::PreprocessorDefine qualityDefine;
        switch ( _quality )
        {
            case QualityPreset::Low:
                qualityDefine = { "FXAA_QUALITY_LOW", "1" };
                break;
            case QualityPreset::Medium:
                qualityDefine = { "FXAA_QUALITY_MEDIUM", "1" };
                break;
            case QualityPreset::High:
                qualityDefine = { "FXAA_QUALITY_HIGH", "1" };
                break;
            case QualityPreset::Ultra:
                qualityDefine = { "FXAA_QUALITY_ULTRA", "1" };
                break;
            default:
                break;
        }

        _shHandle = shaders.shader( "FXAA Shader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Postpro/FXAA.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .addDefine( qualityDefine )
                        .cs( "computeMain" )
                        .load();
    }

    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "FXAA Pipeline" )
                         .shader( _shHandle )
                         .create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder,
                     const Config&                 config ) {

        builder.addPass<Config>( "FXAAPass", config, []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.inputHandle  = pb.read( data.inputHandle ); 
                data.outputHandle = pb.write( data.outputHandle ); }, [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( _pipHandle );

        auto* texIn   = ctx.getTexture( data.inputHandle );
        auto* texOut  = ctx.getTexture( data.outputHandle );
        auto* sampler = ctx.resources.getSampler( data.linearSamplerHandle );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, Graphics::RHI::ResourceState::UnorderedAccess );
        set0->attach( 0, sampler );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        struct FXAAConstants {
            float      subpix;
            float      edgeThreshold;
            float      edgeThresholdMin;
            float      padding0;
            Math::Vec2 inverseTexSize;
        } constants { data.subpix,
                      data.edgeThreshold,
                      data.edgeThresholdMin,
                      0.0f,
                      Math::Vec2( 1.0f / texIn->getDescription().size.width, 1.0f / texIn->getDescription().size.height ) };
        ctx.cmd->pushConstants( 2, constants );

        auto size = texIn->getDescription().size;
        ctx.cmd->dispatch( { ( size.width + 7 ) / 8, ( size.height + 7 ) / 8, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;

    QualityPreset _quality = QualityPreset::High;
};

} // namespace Core::Render
AXION_NAMESPACE_END