/*
 * ==========================================================================================
 * AXION ENGINE - GFX MODULE'S RASTER SAMPLE (SIMPLE TRIANGLE)
 * ==========================================================================================
 * * Author:    Antonio J. Espinosa
 * Date:        2025
 *
 * Description:
 * Entry point for the Raster demonstration. This sample implements a
 * ....
 *
 * Key Features Demonstrated:
 * 
 *
 * ==========================================================================================
 */
#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"

USING_AXION_NAMESPACE

struct Camera {
    Math::Vec3 camPos = { 0.0f, 0.0f, -1.5f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewProj;
    };
};

struct TrianglePass {
    Graphics::PipelineHandle   pipeline;
    Graphics::BufferHandle     vbo;
    Graphics::BufferHandle     ibo;
    Graphics::RGResourceHandle output; // Backbuffer

    Graphics::BufferHandle cameraBuffer; // Camera Uniform Buffer

    struct Data {
        Graphics::RGResourceHandle target;
    };

    void setup( Graphics::RenderPassBuilder& pb, Data& data ) {
        data.target = pb.write( output, Graphics::RHI::ResourceState::RenderTarget );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        auto* pso       = ctx.pipelines.getGraphicPipeline( pipeline );
        auto* targetTex = ctx.getTexture( data.target );
        auto* vb        = ctx.resources.getBuffer( vbo );
        auto* ib        = ctx.resources.getBuffer( ibo );
        auto* ubo       = ctx.resources.getBuffer( cameraBuffer );

        Graphics::RHI::RenderingDesc info;
        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = targetTex } );

        ctx.cmd->beginRendering( info );

        ctx.cmd->bindGraphicPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, ubo, Graphics::RHI::ResourceState::ConstantBuffer );

        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->bindVertexBuffer( 0, vb );
        ctx.cmd->bindIndexBuffer( ib );
        ctx.cmd->drawIndexed( 3 );

        ctx.cmd->endRendering();

        ctx.cmd->barrier( targetTex, Graphics::RHI::ResourceState::Present );
    }
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "GFXRasterSample.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX Raster Sample" } );

        auto       bufferingType    = Graphics::BufferingType::Double;
        const uint FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto       rnd              = Axion::Graphics::createRenderer( wnd,
                                                                       { .gfxApi        = Graphics::API::DirectX12,
                                                                         .bufferingType = bufferingType,
                                                                         .presentMode   = Graphics::PresentMode::Immediate,
                                                                         .autoSync      = true } );

        //-------------------------------------
        // Dedclaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders().shader( "DrawShader" ).asDXIL().path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/Raster.slang" ).vs( "vsMain" ).ps( "psMain" ).load();
        rnd->shaders().compileAllShaders();

        TrianglePass rpass;
        rpass.pipeline = rnd->pipelines()
                             .graphic( "RasterPipeline" )
                             .shader( "DrawShader" )
                             .addRenderTarget( rnd->getSettings().backbufferFormat )
                             .cullNone()
                             .disableDepth()
                             .create();

        //-------------------------------------
        // Declaring Static Resources
        //-------------------------------------

        // GEOMETRY

        struct Vertex {
            float x, y, z;
            float r, g, b;
        };
        std::vector<Vertex> vertices = {
            { 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f },
            { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f } };
        std::vector<uint> indices = { 0, 1, 2 };

        rpass.vbo = rnd->resources()
                        .buffer( "VertexBuffer" )
                        .asVBO()
                        .withData( vertices.data() )
                        .stride( sizeof( Vertex ) )
                        .size( vertices.size() * sizeof( Vertex ) )
                        .create();

        rpass.ibo = rnd->resources().buffer( "IndexBuffer" ).asIBO().withData( indices.data() ).size( indices.size() * sizeof( uint ) ).create();

        //-------------------------------------
        // UNIFORM CONSTANT BUFFER
        //-------------------------------------

        std::vector<Graphics::BufferHandle> camBuffers( FRAMES_IN_FLIGHT );
        for ( int i = 0; i < FRAMES_IN_FLIGHT; ++i )
        {
            camBuffers[i] = rnd->resources().buffer( "CamUniformBuffer_" + std::to_string( i ) ).size( sizeof( Camera::Payload ) ).asCBO().onCPU().create();
        }

        //-------------------------------------
        // CAMERA AND INPUT
        //-------------------------------------

        Camera cam {};

        auto evnt = wnd->onKey().subscribe( [&cam]( const Event::KeyEvent& e ) {
            if ( e.keyCode == Event::KeyCode::W && e.pressed )
                cam.camPos.z += 0.01f;
            if ( e.keyCode == Event::KeyCode::S && e.pressed )
                cam.camPos.z -= 0.01f;
            if ( e.keyCode == Event::KeyCode::D && e.pressed )
                cam.camPos.x += 0.01f;
            if ( e.keyCode == Event::KeyCode::A && e.pressed )
                cam.camPos.x -= 0.01f;
            if ( e.keyCode == Event::KeyCode::Q && e.pressed )
                cam.camPos.y += 0.01f;
            if ( e.keyCode == Event::KeyCode::E && e.pressed )
                cam.camPos.y -= 0.01f;
        } );

        //-------------------------------------
        // Main Loop
        //-------------------------------------

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

            // Process Uniforms

            float aspect = (float)wnd->getSettings().size.width / (float)wnd->getSettings().size.height;
            auto  proj   = Axion::Math::MTX::perspective( Math::radians( cam.fov ), aspect, 0.01f, 10.0f );

            auto            view = Axion::Math::MTX::lookAt( cam.camPos, { 0, 0, 0 }, { 0, 1, 0 } );
            Camera::Payload camData;
            camData.viewProj = proj * view;
            camData.viewProj = Axion::Math::MTX::transpose( camData.viewProj );

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( camBuffers[frameIndex] );
            cbRaw->copyData( camData );

            // Call render func and feed it with a lambda building the RenderGraph
            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                rpass.output       = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );
                rpass.cameraBuffer = camBuffers[frameIndex];

                builder.addPass<TrianglePass>( "TrianglePass", rpass );
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
