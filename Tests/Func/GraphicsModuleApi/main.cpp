#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Platforms/GLFW.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "Engine.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX API TEST" } );
        // auto wnd = Axion::Graphics::createWindowForGLFW(  { .name = "Test Window" } );
        auto rnd = Axion::Graphics::createRenderer( wnd,
                                                    { .gfxApi        = Graphics::API::DirectX12,
                                                      .bufferingType = Graphics::BufferingType::Double,
                                                      .presentMode   = Graphics::PresentMode::Vsync } );

        // Declare Reources
        auto bufferHandle = rnd->resources().buffer( "TestBuffer" ).size( 16 ).create();
        rnd->resources().destroyBuffer( bufferHandle );

        rnd->shaders().shader( "TestShader" ).asDXIL().path( AXION_SHADER_DIR "/Slang/TestShader.slang" ).cs( "computeMain" ).load();
        rnd->shaders().compileShader( "TestShader" );

        auto cmc = rnd->pipelines().compute( "TestPipeline" ).shader( "TestShader" ).create();

        auto gbufferAlbedo = rnd->resources().texture( "GBuffer_Albedo" ).extent( 1920, 1080 ).format( Axion::Graphics::Format::RGBA8_UNORM ).asRenderTarget().create();

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

                // if ( rnd->getTotalFrameNumber() == 0 )
                //     auto simData = builder.buffer( "SimParticles" )
                //                        .size( 1024 * 4 )
                //                        .create();

                TextureHandle    backbufferHandle = rnd->getCurrentBackbufferHandle();
                RGResourceHandle rgBackbuffer     = builder.import( "Backbuffer", backbufferHandle );

                struct PassData {
                    RGResourceHandle target;
                };

                builder.addPass<PassData>( "ClearPass", [&]( RenderPassBuilder& pb, PassData& data ) {
                    data.target = pb.write( rgBackbuffer ); // Declaramos escritura
                },

                                           [&]( const PassData& data, RenderPassContext& ctx ) {
                                            
                auto* tex = ctx.getTexture( data.target );
                
                ctx.cmd->barrier( tex, RHI::ResourceState::RenderTarget );
                ctx.cmd->clearTexture( tex, ClearValue { .color = { 0.4f, 0.6f, 0.9f, 1.0f } } );
                ctx.cmd->barrier( tex, RHI::ResourceState::Present ); } );
            } );
        };

    } catch ( const std::exception& e )
    {
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}
// Graphics::RHI::PipelineLayoutDesc manualLayout = {
//     .sets = {
//         { .bindings = {
//               // binding 0: buffer0 (SRV)
//               {
//                   .binding   = 0,
//                   .type      = Graphics::RHI::DescriptorType::ReadonlyStorageBuffer,
//                   .stageMask = Graphics::RHI::ShaderStage::Compute,
//                   .arraySize = 1 },
//               // binding 1: buffer1 (SRV)
//               {
//                   .binding   = 1,
//                   .type      = Graphics::RHI::DescriptorType::ReadonlyStorageBuffer,
//                   .stageMask = Graphics::RHI::ShaderStage::Compute,
//                   .arraySize = 1 } } },
//         { .bindings = { { { .binding   = 0,
//                             .type      = Graphics::RHI::DescriptorType::StorageBuffer, // RWBuffer suele ser StorageBuffer también
//                             .stageMask = Graphics::RHI::ShaderStage::Compute,
//                             .arraySize = 1 } } } } },
//     .debugName = "Manual_Test_Layout" };
