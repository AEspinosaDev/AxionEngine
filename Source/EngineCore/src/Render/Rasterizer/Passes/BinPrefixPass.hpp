#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Exclusive scan of the bin counts: first slot of each bin in the pixel list and the indirect args of its resolve.
/// A single group with one thread per bin.
class BinPrefixPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inBinCountHandle;
        Graphics::RGResourceHandle outBinOffsetHandle;    ///< First slot of each bin in the pixel list
        Graphics::RGResourceHandle outDispatchArgsHandle; ///< { binID (root constant), groups, 1, 1 } per bin

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr; ///< Frame set (global layout)
        u32                            binCount               = 0;       ///< Bins used this frame (CPU)
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "BinPrefixShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/BinPrefix.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csBinPrefix" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "BinPrefixPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        builder.addPass<Config>( "BinPrefixPass", seedData,
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        // Reading as SRV waits for the classify atomics: the transition is the barrier
        data.inBinCountHandle      = pb.read( data.inBinCountHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.outBinOffsetHandle    = pb.write( data.outBinOffsetHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        data.outDispatchArgsHandle = pb.write( data.outDispatchArgsHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd    = ctx.cmd;
        auto* pso    = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = pso->getDescription().layout;

        cmd->bindComputePipeline( pso );
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Counts in, offsets + resolve args out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inBinCountHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outBinOffsetHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 1 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outDispatchArgsHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        cmd->pushConstants( data.binCount );
        cmd->dispatch( { 1, 1, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
