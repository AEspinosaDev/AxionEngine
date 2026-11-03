#pragma once
#include <Render/DrawIndirect.h>
#include <Render/PassManager.h>
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class IndirectUploadPass : public IRenderPass
{
public:
    struct Config {
        IndirectCommandPayload     indirectData;
        Graphics::RGResourceHandle inOutIndirectBufferHandle;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {

        builder.addPass<Config>( "IndirectUploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) { data.inOutIndirectBufferHandle = pb.write( data.inOutIndirectBufferHandle, Graphics::RHI::ResourceState::CopyDest ); },

                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd          = ctx.cmd;
        auto* activeBuffer = ctx.getBuffer( data.inOutIndirectBufferHandle );

        // Command templates (instance counts = 0) -> GPU buffer, where the culling fills them
        const auto& commands = data.indirectData.commandBufferSlice;
        cmd->copyBuffer( activeBuffer, commands.container, commands.size, 0, commands.offset, Graphics::RHI::BarrierPolicy::None );
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END