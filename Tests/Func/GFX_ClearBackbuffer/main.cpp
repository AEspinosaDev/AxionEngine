#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/Platforms/IGLFW.h"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "Axion/Graphics/IRenderer.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "GFXClearBackbufferTest.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX CLEAR TEST" } );
        auto rnd = Axion::Graphics::createRenderer( wnd.get(),
                                                    { .gfxApi        = Graphics::API::DirectX12,
                                                      .bufferingType = Graphics::BufferingType::Double,
                                                      .presentMode   = Graphics::PresentMode::Vsync } );

        static auto startTime = std::chrono::high_resolution_clock::now();
        while ( !wnd->shouldClose() )
        {
            static uint64_t                           frameCounter   = 0;
            static double                             elapsedSeconds = 0.0;
            static std::chrono::high_resolution_clock clock;
            static auto                               t0 = clock.now();

            frameCounter++;
            auto t1        = clock.now();
            auto deltaTime = t1 - t0;
            t0             = t1;

            elapsedSeconds += deltaTime.count() * 1e-9;
            if ( elapsedSeconds > 1.0 )
            {
                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }

            wnd->processMessages();

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                TextureHandle    backbufferHandle = rnd->getCurrentBackbufferHandle();
                RGResourceHandle rgBackbuffer     = builder.import( "Backbuffer", backbufferHandle );

                struct PassData {
                    RGResourceHandle target;
                };

                builder.addPass<PassData>( "ClearPass", [&]( RenderPassBuilder& pb, PassData& data ) { data.target = pb.write( rgBackbuffer, RHI::ResourceState::RenderTarget ); },

                                           [&]( const PassData& data, RenderPassContext& ctx ) {
                    
                auto* tex = ctx.getTexture( data.target );
                
                ctx.cmd->barrier( tex, RHI::ResourceState::RenderTarget );
                ctx.cmd->clearTexture( tex, ClearValue { .color = { 0.4f, 0.6f, 0.9f, 1.0f } } );
                ctx.cmd->barrier( tex, RHI::ResourceState::Present ); } );
            } );

            float time = std::chrono::duration<float>( t1 - startTime ).count();
            if ( time > 10.0f )
                break;
        };

    } catch ( const std::exception& e )
    {
        AXION_UNUSED_PARAMETER(e);
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}
