#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Zeroes the per bin counters through a copy. D3D12 can't UAV clear structured buffers (clearBuffer).
class BinClearPass : public IRenderPass
{
public:
    struct Config {
        Graphics::RGResourceHandle outBinCountHandle;  ///< Pixels per bin (classify atomics)
        Graphics::RGResourceHandle outBinCursorHandle; ///< Scatter reservations, relative to each bin offset
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        builder.addPass<Config>( "BinClearPass", seedData,
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.outBinCountHandle  = pb.write( data.outBinCountHandle, Graphics::RHI::ResourceState::CopyDest );
        data.outBinCursorHandle = pb.write( data.outBinCursorHandle, Graphics::RHI::ResourceState::CopyDest ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        static constexpr u32 ZEROS[Rasterizer::Config::MAX_RESOLVE_BINS] = {};

        // The CopyDest -> UnorderedAccess transitions of classify and scatter order them after the copies
        ctx.cmd->uploadBuffer( ctx.getBuffer( data.outBinCountHandle ), ZEROS, sizeof( ZEROS ), 0, *ctx.transAllocator, Graphics::RHI::BarrierPolicy::None );
        ctx.cmd->uploadBuffer( ctx.getBuffer( data.outBinCursorHandle ), ZEROS, sizeof( ZEROS ), 0, *ctx.transAllocator, Graphics::RHI::BarrierPolicy::None );
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END
