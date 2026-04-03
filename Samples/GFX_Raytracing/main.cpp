/*
 * ==========================================================================================
 * AXION ENGINE - GFX MODULE'S RAY TRACING SAMPLE (SIMPLE PATH TRACER)
 * ==========================================================================================
 * * Author:    Antonio J. Espinosa
 * Date:        2025
 *
 * Description:
 * Entry point for the Ray Tracing demonstration. This sample implements a
 * progressive Path Tracer rendering the classic Cornell Box scene to showcase Global
 * Illumination capabilities.
 *
 * Key Features Demonstrated:
 * 1. Acceleration Structures: Building BLAS for procedural geometry and TLAS for the scene.
 * 2. Shader Binding Table (SBT): Usage of specialized Hit Groups (Red, Green, White, Light)
 * to handle material properties efficiently without uber-shader divergence.
 * 3. Path Tracing: Monte Carlo integration using Cosine Weighted Hemisphere Sampling for
 * realistic lighting and color bleeding.
 * 4. Temporal Accumulation: Progressive rendering technique to converge noise over multiple
 * frames for high-quality output.
 * 5. Resource Management: Bindless resource setup and interactive orbit camera control.
 *
 * ==========================================================================================
 */
#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/IRenderer.h"
#include "Axion/Graphics/Passes/PostProcess.hpp"
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/Platforms/IGLFW.h"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "cube.h"

USING_AXION_NAMESPACE

struct Scene {
    Math::Vec3 camPos = { 0.0f, 0.0f, -6.0f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewProj;
        Math::Mat4 invView;
        Math::Mat4 invProj;
        Math::Mat4 model;
        u32       frameIndex;
    };
};

struct Cube {

    Graphics::BufferHandle vbo;
    Graphics::BufferHandle ibo;
    Graphics::AccelHandle  accel;

   FixedArray<Vertex, 24> vertices = cubeVertices;
   FixedArray<u32, 36>   indices  = cubeIndices;
};

struct RTXPass {
    Graphics::PipelineHandle rtPipeline;

    Cube cubeData;

    Graphics::RGResourceHandle output;      // ColorBuffer
    Graphics::BufferHandle     uboHandle;   // Camera Uniform Buffer
    Graphics::AccelHandle      accelHandle; // TLAS

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
        // Vertex Pulling
        auto* vb = ctx.resources.getBuffer( cubeData.vbo );
        auto* ib = ctx.resources.getBuffer( cubeData.ibo );
        // UBOs
        auto* ubo = ctx.resources.getBuffer( uboHandle );
        // Accel
        auto* accel = ctx.resources.getAccel( accelHandle );

        ctx.cmd->bindRaytracingPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, accel );
        set0->attach( 1, vb, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 2, ib, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 3, targetTex, Graphics::RHI::ResourceState::UnorderedAccess );
        set0->attach( 4, ubo, Graphics::RHI::ResourceState::ConstantBuffer );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        Graphics::RHI::SBT sbt;
        sbt.setRayGen( "raygenMain" );
        sbt.addMiss( "missMain" );
        sbt.addHitGroup( "GreyMat" );
        sbt.addHitGroup( "RedMat" );
        sbt.addHitGroup( "GreenMat" );
        sbt.addHitGroup( "LightMat" );

        auto sbtView = ctx.allocateSBT( sbt, pso );
        ctx.cmd->dispatchRays( sbtView, targetTex->getDescription().size );
    }
};

Axion::Graphics::RHI::AccelInstanceDesc createInstance(
    u32              id,
    u32              hitGroup,
    const Math::Vec3& pos,
    const Math::Vec3& scale,
    u64             blasAddress );
int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "GFXRaytracingSample.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX Raytracing Sample" } );

        auto       bufferingType    = Graphics::BufferingType::Double;
        const u32 FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto       rnd              = Axion::Graphics::createRenderer( wnd.get(),
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
            .include( AXION_SHADER_DIR "/Slang/Common" )
            .raygen( "raygenMain" )
            .miss( "missMain" )
            .closestHit( "hitGrey" )
            .closestHit( "hitRed" )
            .closestHit( "hitGreen" )
            .closestHit( "hitLight" )
            .load();

        rnd->shaders().compileAllShaders();

        RTXPass rtPass {};
        rtPass.rtPipeline = rnd->pipelines()
                                .raytracing( "RTXPipeline" )
                                .shader( "RTXShader" )
                                .defineHitGroup( "GreyMat", "hitGrey" )
                                .defineHitGroup( "RedMat", "hitRed" )
                                .defineHitGroup( "GreenMat", "hitGreen" )
                                .defineHitGroup( "LightMat", "hitLight" )
                                .setMaxDepth( 1 )
                                .setPayloadSize( sizeof( Axion::Math::Vec4 ) * 4 )
                                .create();

        // CopyPass cpypass {};
        Axion::Graphics::Passes::ToneMapping tnPass {};
        tnPass.init( *rnd );
        Axion::Graphics::Passes::BlitToBackBuffer cpypass {};
        Axion::Graphics::Passes::PresentPass      presentpass {};

        //-------------------------------------
        // Dedclaring Cube Data and Accels
        //-------------------------------------

        // GEOMETRY
        rtPass.cubeData.vbo = rnd->resources()
                                  .buffer( "VertexBuffer" )
                                  .asReadOnlySSBO()
                                  .withData( rtPass.cubeData.vertices.data() )
                                  .stride( sizeof( Vertex ) )
                                  .size( rtPass.cubeData.vertices.size() * sizeof( Vertex ) )
                                  .create();

        rtPass.cubeData.ibo = rnd->resources()
                                  .buffer( "IndexBuffer" )
                                  .asReadOnlySSBO()
                                  .withData( rtPass.cubeData.indices.data() )
                                  .size( rtPass.cubeData.indices.size() * sizeof( u32 ) )
                                  .stride( sizeof( u32 ) )
                                  .create();

        // AS

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
                                                   true )
                                    .instantBuild()
                                    .create();

        // ----------------------------------------
        // CORNELL BOX CREATION
        // ----------------------------------------

        // 2. TLAS (Top Level Acceleration Structure)

        auto*                                                 cubeBlas = rnd->resources().getAccel( rtPass.cubeData.accel );
        STLW::Vector<Axion::Graphics::RHI::AccelInstanceDesc> instances;
        auto                                                  add = cubeBlas->getDeviceAddress();
        // 0. Floor (Grey)
        instances.push_back( createInstance( 0, 0, { 0, -2.0, 0 }, { 4, 0.1f, 4 }, add ) );
        // 1. Ceiling (Grey)
        instances.push_back( createInstance( 1, 0, { 0, 2.0, 0 }, { 4, 0.1f, 4 }, add ) );
        // 2. Centre Wall (Grey)
        instances.push_back( createInstance( 2, 0, { 0, 0, 2.0f }, { 4, 4, 0.1f }, add ) );
        // 3. Left Wall (Red)
        instances.push_back( createInstance( 3, 1, { -2.0f, 0, 0 }, { 0.1f, 4, 4 }, add ) );
        // 4. Right Wall (Green)
        instances.push_back( createInstance( 4, 2, { 2.0f, 0, 0 }, { 0.1f, 4, 4 }, add ) );
        // 5. Light (Emissive)
        instances.push_back( createInstance( 5, 3, { 0, 1.95f, 0 }, { 1.0f, 0.05f, 1.0f }, add ) );
        // 6. Tall Cube
        instances.push_back( createInstance( 6, 0, { -0.5f, -1.6f, 0.5f }, { 0.9f, 1.6f, 0.85f }, add ) );
        // 7. Short Cube
        instances.push_back( createInstance( 7, 0, { 0.5f, -1.7f, -0.5f }, { 0.9f, 0.9f, 0.9f }, add ) );

        rtPass.accelHandle = rnd->resources()
                                 .accel( "TLAS" )
                                 .asTLAS()
                                 .intances( instances )
                                 .instantBuild()
                                 .create();

        // UNIFORM CONSTANT BUFFER
        STLW::Vector<Graphics::BufferHandle>
            scnBuffers( FRAMES_IN_FLIGHT );
        for ( u32 i = 0; i < FRAMES_IN_FLIGHT; ++i )
        {
            scnBuffers[i] = rnd->resources().buffer( "CamUniformBuffer_" + std::to_string( i ) ).size( sizeof( Scene::Payload ) ).asCBO().onCPU().create();
        }

        // CAMERA

        Scene                    scn {};
        static bool              isDragging = false;
        static float             lastX = 0, lastY = 0;
        static float             yaw = 0.0f, pitch = 0.5f;
        static float             radius = 5.0f;
        static Axion::Math::Vec3 target = { 0, 0, 0 };

        // Events
        u64 frameCount = 0;
        auto  evnt       = wnd->onKey().subscribe( [&scn, &frameCount]( const Event::KeyEvent& e ) {
            if ( e.keyCode == Event::KeyCode::W && e.pressed )
            {
                scn.camPos.z += 0.1f;
                frameCount = 0;
            }
            if ( e.keyCode == Event::KeyCode::S && e.pressed )
            {
                scn.camPos.z -= 0.1f;
                frameCount = 0;
            }
            if ( e.keyCode == Event::KeyCode::D && e.pressed )
            {
                scn.camPos.x += 0.1f;

                frameCount = 0;
            }
            if ( e.keyCode == Event::KeyCode::A && e.pressed )
            {
                scn.camPos.x -= 0.1f;
                frameCount = 0;
            }
            if ( e.keyCode == Event::KeyCode::Q && e.pressed )
            {
                scn.camPos.y += 0.1f;
                frameCount = 0;
            }
            if ( e.keyCode == Event::KeyCode::E && e.pressed )
            {
                scn.camPos.y -= 0.1f;
                frameCount = 0;
            }
        } );
        auto  evnt2      = wnd->onResize().subscribe( [&frameCount]( const Event::WindowResizeEvent& e ) {
            frameCount = 0;
        } );

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

            frameCount++;

            float aspect = (float)wnd->getSettings().size.width / (float)wnd->getSettings().size.height;
            auto  proj   = Axion::Math::MTX::perspective( Math::radians( scn.fov ), aspect, 0.01f, 10.0f );

            auto view = Axion::Math::MTX::lookAt( scn.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            Scene::Payload payload;
            payload.viewProj   = proj * view;
            payload.viewProj   = Axion::Math::MTX::transpose( payload.viewProj );
            payload.invView    = Axion::Math::MTX::inverse( Axion::Math::MTX::transpose( view ) );
            payload.invProj    = Axion::Math::MTX::inverse( Axion::Math::MTX::transpose( proj ) );
            payload.model      = Axion::Math::MTX::transpose( Axion::Math::MTX::identity() );
            payload.frameIndex = frameCount;

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( scnBuffers[frameIndex] );
            cbRaw->copyData( payload );

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto rtExtent = wnd->getSettings().size.to3D();

                // 1. Color Buffer (Transient)
                rtPass.output = builder.texture( "ColorBuffer" )
                                    .asStorage()
                                    .format( Format::RGBA8_UNORM )
                                    .extent( rtExtent )
                                    .create();

                rtPass.uboHandle = scnBuffers[frameIndex];

                builder.addPass<RTXPass>( "RTPass", rtPass );

                // tnPass.inputHandle  = rtPass.output;
                // tnPass.outputHandle = builder.texture( "InterBuffer" )
                //                           .asStorage()
                //                           .format( Format::RGBA8_UNORM )
                //                           .extent( rtExtent )
                //                           .create();
                // builder.addPass( "Tonemapping", tnPass );

                auto backbufferHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );

                cpypass.inputHandle  = rtPass.output;
                cpypass.outputHandle = backbufferHandle;
                builder.addPass( "CopyPass", cpypass );

                presentpass.inoutHandle = backbufferHandle;
                builder.addPass( "PresentPass", presentpass );
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

Axion::Graphics::RHI::AccelInstanceDesc createInstance(
    u32              id,
    u32              hitGroup,
    const Math::Vec3& pos,
    const Math::Vec3& scale,
    u64             blasAddress ) {
    Axion::Graphics::RHI::AccelInstanceDesc inst = {};
    inst.instanceID                              = id;
    inst.instanceMask                            = 0xFF;
    inst.hitGroupIndex                           = hitGroup;
    inst.blasDeviceAddress                       = blasAddress;

    auto m         = Axion::Math::MTX::identity();
    m              = Axion::Math::MTX::translate( m, pos );
    m              = Axion::Math::MTX::scale( m, scale );
    inst.transform = Axion::Math::MTX::transpose( m ); // Row-Major DXR

    return inst;
}
