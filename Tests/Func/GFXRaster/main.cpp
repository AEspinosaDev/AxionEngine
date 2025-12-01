#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Platforms/GLFW.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"

USING_AXION_NAMESPACE

struct TrianglePass {
    Graphics::PipelineHandle   pipeline;
    Graphics::BufferHandle     vbo;
    Graphics::BufferHandle     ibo;
    Graphics::RGResourceHandle output; // Backbuffer

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

        Graphics::RHI::RenderingDesc info;
        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture    = targetTex,
                                           .clearValue = { .color = { 0.5f, 0.5f, 0.5f, 1.0f } } } );

        ctx.cmd->beginRendering( info );

        ctx.cmd->bindGraphicPipeline( pso );

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
        Axion::Logger::init( Logger::Level::Info, "Engine.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX RASTER TEST" } );
        // auto wnd = Axion::Graphics::createWindowForGLFW(  { .name = "GFX COMPUTE TEST" } );

        auto rnd = Axion::Graphics::createRenderer( wnd,
                                                    { .gfxApi        = Graphics::API::DirectX12,
                                                      .bufferingType = Graphics::BufferingType::Double,
                                                      .presentMode   = Graphics::PresentMode::Immediate,
                                                      .autoSync      = true } );

        //-------------------------------------
        // Dedclaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders().shader( "DrawShader" ).asDXIL().path( AXION_SHADER_DIR "/Slang/Testing/Raster.slang" ).vs( "vsMain" ).ps( "psMain" ).load();
        rnd->shaders().compileAllShaders();

        TrianglePass rpass;
        rpass.pipeline = rnd->pipelines().graphic( "RasterPipeline" ).shader( "DrawShader" ).addRenderTarget( rnd->getSettings().backbufferFormat ).cullNone().disableDepth().create();

        // auto evnt = wnd->onKey().subscribe( [&rpass]( const Event::KeyEvent& e ) {
        //     if ( e.keyCode == 38 && e.pressed ){
        //     rpass.pushData.speed += 0.1;
        // }
        //     if ( e.keyCode == 40 && e.pressed ){
        //     rpass.pushData.speed -= 0.1;

        // } } );

        //-------------------------------------
        // Dedclaring Static Resources
        //-------------------------------------

        struct Vertex {
            float x, y, z;
            float r, g, b;
        };
        std::vector<Vertex> vertices = {
            { 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f },
            { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f } };
        std::vector<uint> indices = { 0, 1, 2 };

        auto vboHandle = rnd->resources().buffer( "VertexBuffer" ).asVBO().withData( vertices.data() ).stride( sizeof( Vertex ) ).size( vertices.size() * sizeof( Vertex ) ).create();
        auto iboHandle = rnd->resources().buffer( "IndexBuffer" ).asIBO().withData( indices.data() ).size( indices.size() * sizeof( uint ) ).create();
        rpass.vbo      = vboHandle;
        rpass.ibo      = iboHandle;

        //-------------------------------------
        // Main Loop
        //-------------------------------------

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

                rpass.output = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );

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
