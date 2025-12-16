#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Platforms/GLFW.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"
#include "cube.h"
USING_AXION_NAMESPACE

struct Camera {
    Math::Vec3 camPos = { 0.0f, 0.0f, -2.0f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewModelProj;
    };
};

struct Cube {
    Graphics::BufferHandle vbo;
    Graphics::BufferHandle ibo;

    Graphics::TextureHandle texture;
    Graphics::SamplerHandle sampler;

    std::vector<Vertex> vertices = cubeVertices;
    std::vector<uint>   indices  = cubeIndices;
};

struct ForwardPass {
    Graphics::PipelineHandle pipeline;

    Cube cubeData = {};

    Graphics::RGResourceHandle output;      // ColorBuffer
    Graphics::RGResourceHandle depthOutput; // DepthBuffer

    Graphics::BufferHandle cameraBuffer; // Camera Uniform Buffer

    struct Data {
        Graphics::RGResourceHandle target;
        Graphics::RGResourceHandle depthTarget;
    };

    void setup( Graphics::RenderPassBuilder& pb, Data& data ) {
        data.target      = pb.write( output, Graphics::RHI::ResourceState::RenderTarget );
        data.depthTarget = pb.write( depthOutput, Graphics::RHI::ResourceState::DepthWrite );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        // PSO
        auto* pso = ctx.pipelines.getGraphicPipeline( pipeline );

        // RTs
        auto* targetTex = ctx.getTexture( data.target );
        auto* depthTex  = ctx.getTexture( data.depthTarget );

        // Cube Related
        auto* vb      = ctx.resources.getBuffer( cubeData.vbo );
        auto* ib      = ctx.resources.getBuffer( cubeData.ibo );
        auto* ubo     = ctx.resources.getBuffer( cameraBuffer );
        auto* sampler = ctx.resources.getSampler( cubeData.sampler );
        auto* texture = ctx.resources.getTexture( cubeData.texture );

        Graphics::RHI::RenderingDesc info;

        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = targetTex } );
        info.depthStencilAttachment = { .texture = depthTex };

        ctx.cmd->beginRendering( info );
        ctx.cmd->bindGraphicPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, ubo, Graphics::RHI::ResourceState::ConstantBuffer );
        set0->attach( 1, texture, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 0, sampler );

        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->bindVertexBuffer( 0, vb );
        ctx.cmd->bindIndexBuffer( ib );
        ctx.cmd->drawIndexed( (uint)cubeData.indices.size() );

        ctx.cmd->endRendering();
    }
};

struct ToneMappingPass {

    Graphics::PipelineHandle   pipeline;
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
        auto* pso = ctx.pipelines.getComputePipeline( pipeline );

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

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX SAMPLER TEST" } );

        auto       bufferingType    = Graphics::BufferingType::Double;
        const uint FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto       rnd              = Axion::Graphics::createRenderer( wnd,
                                                                       { .gfxApi        = Graphics::API::DirectX12,
                                                                         .bufferingType = bufferingType,
                                                                         .presentMode   = Graphics::PresentMode::Immediate,
                                                                         .autoSync      = true } );

        //-------------------------------------
        // Declaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders()
            .shader( "DrawShader" )
            .asDXIL()
            .path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/Samplers.slang" )
            .vs( "vsMain" )
            .ps( "psMain" )
            .load();

        rnd->shaders()
            .shader( "TonemappingShader" )
            .asDXIL()
            .path( AXION_SHADER_DIR "/Slang/Postpro/Tonemapping.slang" )
            .include( AXION_SHADER_DIR "/Slang/Common" )
            .cs( "computeMain" )
            .load();

        rnd->shaders().compileAllShaders();

        ForwardPass fwPass {};
        fwPass.pipeline = rnd->pipelines()
                              .graphic( "FwPipeline" )
                              .shader( "DrawShader" )
                              .addRenderTarget( Axion::Graphics::Format::RGBA16_FLOAT ) // HDR Format
                              .setDepthFormat( Graphics::Format::D32 )                  // Depth Format
                              .cullNone()                                               // Enable culling later if needed
                              .create();

        ToneMappingPass tmPass {};
        tmPass.pipeline = rnd->pipelines().compute( "TmPipeline" ).shader( "TonemappingShader" ).create();

        CopyPass cpypass;

        //-------------------------------------
        // Dedclaring Static Resources
        //-------------------------------------

        // TEXTURE
        auto imageData          = Axion::Helpers::loadImage( AXION_SAMPLES_RESOURCE_DIR "/Misc/Axion.png" );
        fwPass.cubeData.texture = rnd->resources()
                                      .texture( "CubeTexture" )
                                      .format( Axion::Graphics::Format::RGBA8_UNORM )
                                      .extent( imageData.width, imageData.height, 1 )
                                      .withData( imageData.getData() )
                                      .create();
        fwPass.cubeData.sampler = rnd->resources().sampler( "LinearSampler" ).create();

        // GEOMETRY
        fwPass.cubeData.vbo = rnd->resources()
                                  .buffer( "VertexBuffer" )
                                  .asVBO()
                                  .withData( fwPass.cubeData.vertices.data() )
                                  .stride( sizeof( Vertex ) )
                                  .size( fwPass.cubeData.vertices.size() * sizeof( Vertex ) )
                                  .create();

        fwPass.cubeData.ibo = rnd->resources()
                                  .buffer( "IndexBuffer" )
                                  .asIBO()
                                  .withData( fwPass.cubeData.indices.data() )
                                  .size( fwPass.cubeData.indices.size() * sizeof( uint ) )
                                  .create();

        // UNIFORM CONSTANT BUFFER
        std::vector<Graphics::BufferHandle> camBuffers( FRAMES_IN_FLIGHT );
        for ( uint i = 0; i < FRAMES_IN_FLIGHT; ++i )
        {
            camBuffers[i] = rnd->resources().buffer( "CamUniformBuffer_" + std::to_string( i ) ).size( sizeof( Camera::Payload ) ).asCBO().onCPU().create();
        }

        // CAMERA

        Camera cam {};

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

            float aspect = (float)wnd->getSettings().size.width / (float)wnd->getSettings().size.height;
            auto  proj   = Axion::Math::perspective( Math::radians( cam.fov ), aspect, 0.01f, 10.0f );

            auto view = Axion::Math::lookAt( cam.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            float time  = std::chrono::duration<float>( t1 - startTime ).count();
            auto  model = Axion::Math::identity();
            model       = Axion::Math::rotate( model, time * 1.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
            model       = Axion::Math::rotate( model, time * 0.5f, Math::Vec3( 1.0f, 0.0f, 0.0f ) );

            Camera::Payload camData;
            camData.viewModelProj = proj * view * model;
            camData.viewModelProj = Axion::Math::transpose( camData.viewModelProj );

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( camBuffers[frameIndex] );
            cbRaw->copyData( camData );

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto rtExtent = wnd->getSettings().size.to3D();

                // 1. HDR Color Buffer (Transient)
                fwPass.output = builder.texture( "ColorBuffer" )
                                    .asRenderTarget()
                                    .asStorage()                    // Allow reading as SRV in next pass
                                    .format( Format::RGBA16_FLOAT ) // HDR
                                    .extent( rtExtent )
                                    .clearValue( { .color = { 0.2f, 0.2f, 0.2f, 1.0f } } )
                                    .create();

                // 2. Depth Buffer (Transient)
                fwPass.depthOutput = builder.texture( "DepthBuffer" )
                                         .asDepthStencil()
                                         .format( Format::D32 ) // Explicit Depth Format
                                         .extent( rtExtent )
                                         .create();
                fwPass.cameraBuffer = camBuffers[frameIndex];

                builder.addPass<ForwardPass>( "ForwardPass", fwPass );

                tmPass.inputHandle  = fwPass.output;
                tmPass.outputHandle = builder.texture( "LDRIntermidiate" ).format( Format::RGBA8_UNORM ).extent( rtExtent ).asStorage().create();

                builder.addPass<ToneMappingPass>( "TonemappingPass", tmPass );

                cpypass.inputHandle  = tmPass.outputHandle;
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
