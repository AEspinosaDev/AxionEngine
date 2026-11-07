#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Writes the packed coordinates of every pixel inside the slot range of its bin.
/// Each tile reserves one contiguous block per bin, so the pixels of a tile stay together in the list.
class BinScatterPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle inBinImageHandle;
        Graphics::RGResourceHandle inBinOffsetHandle;    ///< First slot of each bin
        Graphics::RGResourceHandle inOutBinCursorHandle; ///< Reserved slots per bin, relative to its offset (zeroed by BinClearPass)
        Graphics::RGResourceHandle outPixelListHandle;   ///< x | y << 16

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr; ///< Frame set: gFrame (resolution)
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "BinScatterShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/BinScatter.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csBinScatter" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "BinScatterPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        builder.addPass<Config>( "BinScatterPass", seedData,
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.inBinImageHandle     = pb.read( data.inBinImageHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inBinOffsetHandle    = pb.read( data.inBinOffsetHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inOutBinCursorHandle = pb.write( data.inOutBinCursorHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        data.outPixelListHandle   = pb.write( data.outPixelListHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd      = ctx.cmd;
        auto* pso      = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout   = pso->getDescription().layout;
        auto* binImage = ctx.getTexture( data.inBinImageHandle );

        cmd->bindComputePipeline( pso );
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Bins + offsets in, cursors + pixel list out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::texture( 0 ), Graphics::RHI::DescriptorType::SRV_Image, binImage );
        ioSet->attach( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inBinOffsetHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.inOutBinCursorHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 1 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outPixelListHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        const auto size = binImage->getDescription().size;
        const u32  tile = Rasterizer::Config::BINNING_TILE_SIZE;
        cmd->dispatch( { ( size.width + tile - 1 ) / tile, ( size.height + tile - 1 ) / tile, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
