#pragma once
#include "Axion/Graphics/Passes/Recipe.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::Passes {

#pragma region Blit

struct BlitData {
    RGResourceHandle input;
    RGResourceHandle output;
};

struct BlitToBackBuffer : public IPassRecipe<BlitData> {

    PipelineHandle pipelineHandle;

    RGResourceHandle inputHandle;
    RGResourceHandle outputHandle;

    void init( IRenderer& rnd ) override {}

    void setup( RenderPassBuilder& builder, BlitData& data ) override {
        data.input  = builder.read( inputHandle, RHI::ResourceState::CopySource );
        data.output = builder.write( outputHandle, RHI::ResourceState::CopySource );
    }

    void execute( const BlitData& data, RenderPassContext& ctx ) override {
        auto* srcTex = ctx.getTexture( data.input );
        auto* dstTex = ctx.getTexture( data.output );

        ctx.cmd->copyTexture( dstTex, srcTex );

        ctx.cmd->barrier( dstTex, Graphics::RHI::ResourceState::Present );
    }
};

#pragma endregion

} // namespace Graphics::Passes

AXION_NAMESPACE_END
