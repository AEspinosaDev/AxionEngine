#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Passes/PostProcess.hpp"
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
    Graphics::AccelHandle  accel;

    std::vector<Vertex> vertices = cubeVertices;
    std::vector<uint>   indices  = cubeIndices;
};

struct RTXPass {
    Graphics::PipelineHandle rtPipeline;

    Cube cubeData;

    Graphics::RGResourceHandle output;       // ColorBuffer
    Graphics::BufferHandle     cameraBuffer; // Camera Uniform Buffer
    Graphics::AccelHandle      accelHandle;  // TLAS

    struct Data {
        Graphics::RGResourceHandle target;
    };

    void setup( Graphics::RenderPassBuilder& pb, Data& data ) {
        data.target = pb.write( output, Graphics::RHI::ResourceState::UnorderedAccess );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        // PSO
        auto* pso = ctx.pipelines.getRaytracingPipeline( rtPipeline );
        // RTs
        auto* targetTex = ctx.getTexture( data.target );
        // UBOs
        // auto* ubo = ctx.resources.getBuffer( cameraBuffer );
        // Accel
        auto* accel = ctx.resources.getAccel( accelHandle );

        ctx.cmd->bindRaytracingPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, accel );
        set0->attach( 1, targetTex, Graphics::RHI::ResourceState::UnorderedAccess );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        Graphics::RHI::SBT sbt;
        sbt.setRayGen( "raygenMain" );
        sbt.addMiss( "missMain" );
        sbt.addHitGroup( "RedGroup" );

        auto sbtView = ctx.allocateSBT( sbt, pso );
        ctx.cmd->dispatchRays( sbtView, targetTex->getDescription().size );
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

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX Raytracing Sample" } );

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
            .shader( "RTXShader" )
            .asDXIL()
            .path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/Raytracing.slang" )
            .raygen( "raygenMain" )
            .miss( "missMain" )
            .closestHit( "hitRedMain" )
            .load();

        rnd->shaders().compileAllShaders();

        RTXPass rtPass {};
        rtPass.rtPipeline = rnd->pipelines()
                                .raytracing( "RTXPipeline" )
                                .shader( "RTXShader" )
                                .defineHitGroup( "RedGroup", "hitRedMain" )
                                .setMaxDepth( 1 )
                                .setPayloadSize( sizeof( Axion::Math::Vec4 ) )
                                .create();

        CopyPass cpypass {};

        //-------------------------------------
        // Dedclaring Static Resources
        //-------------------------------------

        // GEOMETRY
        rtPass.cubeData.vbo = rnd->resources()
                                  .buffer( "VertexBuffer" )
                                  .asVBO()
                                  .withData( rtPass.cubeData.vertices.data() )
                                  .stride( sizeof( Vertex ) )
                                  .size( rtPass.cubeData.vertices.size() * sizeof( Vertex ) )
                                  .create();

        rtPass.cubeData.ibo = rnd->resources()
                                  .buffer( "IndexBuffer" )
                                  .asIBO()
                                  .withData( rtPass.cubeData.indices.data() )
                                  .size( rtPass.cubeData.indices.size() * sizeof( uint ) )
                                  .create();

        // AS
        // 1. BLAS (Bottom Level Acceleration Structure)
        // Using the new overloaded method to look up buffers by name
        auto* vb = rnd->resources().getBuffer( rtPass.cubeData.vbo );
        auto* ib = rnd->resources().getBuffer( rtPass.cubeData.ibo );

        rtPass.cubeData.accel = rnd->resources()
                                    .accel( "Cube BLAS" )
                                    .asBLAS()
                                    .withGeometry( vb->getDeviceAddress(),
                                                   rtPass.cubeData.vertices.size(),
                                                   vb->getDescription().stride,
                                                   Axion::Graphics::Format::RGB32_FLOAT,
                                                   ib->getDeviceAddress(),
                                                   rtPass.cubeData.indices.size(),
                                                   true ).create();

        // 2. TLAS (Top Level Acceleration Structure)
        // We need the BLAS device address to create an instance
        auto* cubeBlas = rnd->resources().getAccel( rtPass.cubeData.accel );

        Axion::Graphics::RHI::AccelInstanceDesc instance = {};
        instance.instanceID                              = 0;
        instance.instanceMask                            = 0xFF;                    // Visible to all
        instance.transform                               = Axion::Math::identity(); // Identity matrix (World Position)
        // instance.flags                                   = Graphics::Raytracing::InstanceFlags::None;
        instance.hitGroupIndex     = 0;
        instance.blasDeviceAddress = cubeBlas->getDeviceAddress();

        rtPass.accelHandle = rnd->resources()
                              .accel( "TLAS" )
                              .asTLAS()
                              .withInstance( instance ) // Pass the instance description
                              .create();

        // UNIFORM CONSTANT BUFFER
        std::vector<Graphics::BufferHandle>
            camBuffers( FRAMES_IN_FLIGHT );
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
            // model       = Axion::Math::rotate( model, time * 1.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
            // model       = Axion::Math::rotate( model, time * 0.5f, Math::Vec3( 1.0f, 0.0f, 0.0f ) );

            Camera::Payload camData;
            camData.viewModelProj = proj * view * model;
            camData.viewModelProj = Axion::Math::transpose( camData.viewModelProj );

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( camBuffers[frameIndex] );
            cbRaw->copyData( camData );

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto rtExtent = wnd->getSettings().size.to3D();

                // 1. Color Buffer (Transient)
                rtPass.output = builder.texture( "ColorBuffer" )
                                    .asStorage()
                                    .format( Format::RGBA8_UNORM )
                                    .extent( rtExtent )
                                    .create();

                rtPass.cameraBuffer = camBuffers[frameIndex];

                builder.addPass<RTXPass>( "RTPass", rtPass );

                cpypass.inputHandle  = rtPass.output;
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
