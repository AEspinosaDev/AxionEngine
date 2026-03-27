#pragma once
#include "Axion/Graphics/Passes/IRecipe.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::Passes {


#pragma region Present

struct PresentPassData {
    RGResourceHandle inoutHandle;
};

struct PresentPass : public IPassRecipe<PresentPassData> {

    RGResourceHandle inoutHandle;

    PresentPass() {}
    PresentPass( RGResourceHandle inout ) : inoutHandle( inout ) {}

    void init( IRenderer& /*rnd*/ ) override {}

    void setup( RenderPassBuilder& builder, PresentPassData& data ) override {
        data.inoutHandle = builder.write( inoutHandle, RHI::ResourceState::Present );
    }

    void execute( const PresentPassData& /*data*/, RenderPassContext& /*ctx*/ ) override {
    }
};

#pragma endregion

#pragma region Blit

struct BlitData {
    RGResourceHandle input;
    RGResourceHandle output;
};

struct BlitToBackBuffer : public IPassRecipe<BlitData> {

    RGResourceHandle inputHandle;
    RGResourceHandle outputHandle;

    BlitToBackBuffer() {}
    BlitToBackBuffer( RGResourceHandle input, RGResourceHandle output )
        : inputHandle( input )
        , outputHandle( output ) {}

    void init( IRenderer& /*rnd*/ ) override {}

    void setup( RenderPassBuilder& builder, BlitData& data ) override {
        data.input  = builder.read( inputHandle, RHI::ResourceState::CopySource );
        data.output = builder.write( outputHandle, RHI::ResourceState::CopyDest );
    }

    void execute( const BlitData& data, RenderPassContext& ctx ) override {
        auto* srcTex = ctx.getTexture( data.input );
        auto* dstTex = ctx.getTexture( data.output );

        ctx.cmd->copyTexture( dstTex, srcTex );

    }
};

#pragma endregion
#pragma region GUI

struct GUIPassData {
    RGResourceHandle output;
};

struct GUIPass : public IPassRecipe<GUIPassData> {

    // Target texture (usually the Backbuffer / Swapchain image)
    RGResourceHandle        outputHandle;
    const RHI::IGUIBackend* guiBackend = nullptr;

    GUIPass() {}
    GUIPass( const RHI::IGUIBackend* backend, RGResourceHandle target )
        : guiBackend( backend )
        , outputHandle( target ) {}

    void init( IRenderer& /*rnd*/ ) override {
    }

    void setup( RenderPassBuilder& builder, GUIPassData& data ) override {
        data.output = builder.write( outputHandle, RHI::ResourceState::RenderTarget );
    }

    void execute( const GUIPassData& data, RenderPassContext& ctx ) override {
        if ( !guiBackend )
            return;
        auto* texOut = ctx.getTexture( data.output );

        RHI::RenderingDesc info;
        info.renderArea = texOut->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = texOut, .loadOp = RHI::LoadOp::Load } );

        ctx.cmd->beginRendering( info );

        guiBackend->render( ctx.cmd );

        ctx.cmd->endRendering();

    }
};

#pragma endregion

} // namespace Graphics::Passes

AXION_NAMESPACE_END
