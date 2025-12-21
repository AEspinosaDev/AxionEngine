/*
 * ==========================================================================================
 * AXION ENGINE - GFX MODULE'S COMPUTE SAMPLE
 * ==========================================================================================
 * * Author:    Antonio J. Espinosa
 * Date:        2025
 *
 * Description:
 * Entry point for the Compute demonstration. This sample implements a
 * ....
 *
 * Key Features Demonstrated:
 *
 *
 * ==========================================================================================
 */
#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Platforms/GLFW.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"

USING_AXION_NAMESPACE

struct GenerationPass {

    Graphics::PipelineHandle   pipelineHandle;
    Graphics::RGResourceHandle outputHandle;
    struct PushConstantData {
        float time;
        float speed = 1.0;
    };
    PushConstantData pushData;

    struct Data {
        Graphics::RGResourceHandle outputHDR;
    };

    void setup( Graphics::RenderPassBuilder& builder, Data& data ) {
        data.outputHDR = builder.write( outputHandle );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( pipelineHandle );

        auto* texOut = ctx.getTexture( data.outputHDR );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );
        ctx.cmd->pushConstants( 1, pushData );

        ctx.cmd->dispatch( texOut->getDescription().size );
    }
};

struct ToneMappingPass {

    Graphics::PipelineHandle   pipelineHandle;
    Graphics::RGResourceHandle inputHandle;
    Graphics::RGResourceHandle outputHandle;

    struct Data {
        Graphics::RGResourceHandle inputHDR;
        Graphics::RGResourceHandle outputLDR;
    };

    void setup( Graphics::RenderPassBuilder& builder, Data& data ) {
        data.inputHDR  = builder.read( inputHandle );
        data.outputLDR = builder.write( outputHandle );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        auto* pso = ctx.pipelines.getComputePipeline( pipelineHandle );

        auto* texIn  = ctx.getTexture( data.inputHDR );
        auto* texOut = ctx.getTexture( data.outputLDR );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->dispatch( texIn->getDescription().size );
    }
};

struct CopyPass {

    Graphics::RGResourceHandle inputHandle;
    Graphics::RGResourceHandle outputHandle;

    struct Data {
        Graphics::RGResourceHandle inputLDR;
        Graphics::RGResourceHandle outputLDR;
    };

    void setup( Graphics::RenderPassBuilder& builder, Data& data ) {
        data.inputLDR  = builder.read( inputHandle, Graphics::RHI::ResourceState::CopySource );
        data.outputLDR = builder.write( outputHandle, Graphics::RHI::ResourceState::CopyDest );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {

        auto* srcTex = ctx.getTexture( data.inputLDR );
        auto* dstTex = ctx.getTexture( data.outputLDR );

        ctx.cmd->copyTexture( dstTex, srcTex );

        ctx.cmd->barrier( dstTex, Graphics::RHI::ResourceState::Present );
    }
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "Engine.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX Compute Sample" } );
        // auto wnd = Axion::Graphics::createWindowForGLFW(  { .name = "GFX COMPUTE TEST" } );

        auto rnd = Axion::Graphics::createRenderer( wnd,
                                                    { .gfxApi        = Graphics::API::DirectX12,
                                                      .bufferingType = Graphics::BufferingType::Double,
                                                      .presentMode   = Graphics::PresentMode::Immediate,
                                                      .autoSync      = true } );

        //-------------------------------------
        // Dedclaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders().shader( "GenerationShader" ).asDXIL().path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/Generation.slang" ).cs( "computeMain" ).load();
        rnd->shaders()
            .shader( "TonemappingShader" )
            .asDXIL()
            .path( AXION_SHADER_DIR "/Slang/Postpro/Tonemapping.slang" )
            .include( AXION_SHADER_DIR "/Slang/Common" )
            .cs( "computeMain" )
            .load();
        rnd->shaders().compileAllShaders();

        GenerationPass gpass;
        gpass.pipelineHandle = rnd->pipelines().compute( "GenerationPipeline" ).shader( "GenerationShader" ).create();
        ToneMappingPass tpass;
        tpass.pipelineHandle = rnd->pipelines().compute( "TonemappingPipeline" ).shader( "TonemappingShader" ).create();
        CopyPass cpypass;

        // Subscribe Input Events
        auto evnt = wnd->onKey().subscribe( [&gpass]( const Event::KeyEvent& e ) { 
            if ( e.keyCode == Event::KeyCode::Up && e.pressed ){
            gpass.pushData.speed += 0.1;
        }
            if ( e.keyCode == Event::KeyCode::Down && e.pressed ){
            gpass.pushData.speed -= 0.1;

        } } );

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
                wchar_t buffer[100];
                double  fps = frameCounter / elapsedSeconds;
                swprintf_s( buffer, 100, L"FPS: %.2f\n", fps );

                OutputDebugStringW( buffer );

                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }

            wnd->processMessages();

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto wndExtent      = wnd->getSettings().size;
                gpass.pushData.time = std::chrono::duration<float>( t1 - startTime ).count();
                gpass.outputHandle  = builder.texture( "HDRIntermidiate" ).format( Format::RGBA16_FLOAT ).extent( wndExtent.width, wndExtent.height, 1 ).asStorage().create();
                builder.addPass<GenerationPass>( "GenerationPass", gpass );

                tpass.outputHandle = builder.texture( "LDRIntermidiate" ).format( Format::RGBA8_UNORM ).extent( wndExtent.width, wndExtent.height, 1 ).asStorage().create();
                tpass.inputHandle  = gpass.outputHandle;
                builder.addPass<ToneMappingPass>( "TonemappingPass", tpass );

                cpypass.inputHandle  = tpass.outputHandle;
                cpypass.outputHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );
                builder.addPass<CopyPass>( "CopyPass", cpypass );
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
