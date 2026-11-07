#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Classifies every pixel into its resolve bin and counts the pixels of each bin.
/// Background pixels are resolved here: they belong to no bin.
class BinClassifyPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inVisHandle;
        Graphics::BufferSlice      inBundleToBinSlice; ///< psoBundleID -> bin (CPU upload)

        Graphics::RGResourceHandle outBinImageHandle; ///< Bin of every pixel (0xFF background)
        Graphics::RGResourceHandle outBinCountHandle; ///< Pixels per bin: tile histogram + one atomic per bin and tile
        Graphics::RGResourceHandle outColorHandle;    ///< Only the background is written here

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr; ///< Frame set: gFrame, gInstances, gMaterials, gParticleSystems
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "BinClassifyShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/BinClassify.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csBinClassify" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "BinClassifyPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        builder.addPass<Config>( "BinClassifyPass", seedData,
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.inVisHandle       = pb.read( data.inVisHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.outBinImageHandle = pb.write( data.outBinImageHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        data.outBinCountHandle = pb.write( data.outBinCountHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        data.outColorHandle    = pb.write( data.outColorHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd    = ctx.cmd;
        auto* pso    = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = pso->getDescription().layout;
        auto* vis    = ctx.getTexture( data.inVisHandle );

        cmd->bindComputePipeline( pso );

        // SPACE 0: Frame data (instances, materials, particle systems)
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Vis buffer + bundle table in, bins + counts + background out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::texture( 0 ), Graphics::RHI::DescriptorType::SRV_Image, vis );
        ioSet->attachBufferSlice( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, data.inBundleToBinSlice );
        ioSet->attach( Graphics::PassIO::rwTexture( 0 ), Graphics::RHI::DescriptorType::UAV_Image, ctx.getTexture( data.outBinImageHandle ) );
        ioSet->attach( Graphics::PassIO::rwTexture( 1 ), Graphics::RHI::DescriptorType::UAV_Image, ctx.getTexture( data.outColorHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outBinCountHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        const auto size = vis->getDescription().size;
        const u32  tile = Rasterizer::Config::BINNING_TILE_SIZE;
        cmd->dispatch( { ( size.width + tile - 1 ) / tile, ( size.height + tile - 1 ) / tile, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
