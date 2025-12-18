#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Passes/PostProcess.hpp"
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/Platforms/GLFW.h"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/Renderer.h"
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
        uint       frameIndex;
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
        // UBOs
        auto* ubo = ctx.resources.getBuffer( uboHandle );
        // Accel
        auto* accel = ctx.resources.getAccel( accelHandle );

        ctx.cmd->bindRaytracingPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, accel );
        set0->attach( 1, targetTex, Graphics::RHI::ResourceState::UnorderedAccess );
        set0->attach( 2, ubo, Graphics::RHI::ResourceState::ConstantBuffer );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        Graphics::RHI::SBT sbt;
        sbt.setRayGen( "raygenMain" );
        sbt.addMiss( "missMain" );
        sbt.addHitGroup( "MainGroup" );

        auto sbtView = ctx.allocateSBT( sbt, pso );
        ctx.cmd->dispatchRays( sbtView, targetTex->getDescription().size );
    }
};

Axion::Graphics::RHI::AccelInstanceDesc createInstance(
    uint              id,
    uint              hitGroup,
    const Math::Vec3& pos,
    const Math::Vec3& scale,
    const Math::Vec3& color,
    ulong             blasAddress );
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
            .include( AXION_SHADER_DIR "/Slang/Common" )
            .raygen( "raygenMain" )
            .miss( "missMain" )
            .closestHit( "closestHit" )
            .load();

        rnd->shaders().compileAllShaders();

        RTXPass rtPass {};
        rtPass.rtPipeline = rnd->pipelines()
                                .raytracing( "RTXPipeline" )
                                .shader( "RTXShader" )
                                .defineHitGroup( "MainGroup", "closestHit" )
                                .setMaxDepth( 1 )
                                .setPayloadSize( sizeof( Axion::Math::Vec4 ) * 4 )
                                .create();

        // CopyPass cpypass {};
        Axion::Graphics::Passes::ToneMapping tnPass {};
        tnPass.init( *rnd );
        Axion::Graphics::Passes::BlitToBackBuffer cpypass {};

        //-------------------------------------
        // Dedclaring Cube Data and Accels
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
                                    .create();

        // ----------------------------------------
        // CORNELL BOX CREATION
        // ----------------------------------------

        // 2. TLAS (Top Level Acceleration Structure)

        auto*                                                cubeBlas = rnd->resources().getAccel( rtPass.cubeData.accel );
        std::vector<Axion::Graphics::RHI::AccelInstanceDesc> instances;
        auto                                                 add = cubeBlas->getDeviceAddress();
        // 0. Floor (Grey)
        instances.push_back( createInstance( 0, 0, { 0, -2.0, 0 }, { 4, 0.1f, 4 }, { 0.8f, 0.8f, 0.8f }, add ) );
        // 1. Ceiling (Grey)
        instances.push_back( createInstance( 1, 0, { 0, 2.0, 0 }, { 4, 0.1f, 4 }, { 0.8f, 0.8f, 0.8f }, add ) );
        // 2. Centre Wall (Grey)
        instances.push_back( createInstance( 2, 0, { 0, 0, 2.0f }, { 4, 4, 0.1f }, { 0.8f, 0.8f, 0.8f }, add ) );
        // 3. Left Wall (Red)
        instances.push_back( createInstance( 3, 0, { -2.0f, 0, 0 }, { 0.1f, 4, 4 }, { 0.8f, 0.1f, 0.1f }, add ) );
        // 4. Right Wall (Green)
        instances.push_back( createInstance( 4, 0, { 2.0f, 0, 0 }, { 0.1f, 4, 4 }, { 0.1f, 0.8f, 0.1f }, add ) );
        // 5. Light (Emissive)
        instances.push_back( createInstance( 5, 0, { 0, 1.95f, 0 }, { 1.0f, 0.05f, 1.0f }, { 50.0f, 50.0f, 50.0f }, add ) );
        // 6. Tall Cube
        instances.push_back( createInstance( 6, 0, { -0.5f, -1.6f, 0.5f }, { 0.9f, 1.6f, 0.85f }, { 0.8f, 0.8f, 0.8f }, add ) );
        // 7. Short Cube
        instances.push_back( createInstance( 7, 0, { 0.5f, -1.7f, -0.5f }, { 0.9f, 0.9f, 0.9f }, { 0.8f, 0.8f, 0.8f }, add ) );

        rtPass.accelHandle = rnd->resources()
                                 .accel( "TLAS" )
                                 .asTLAS()
                                 .intances( instances )
                                 .create();

        // UNIFORM CONSTANT BUFFER
        std::vector<Graphics::BufferHandle>
            scnBuffers( FRAMES_IN_FLIGHT );
        for ( uint i = 0; i < FRAMES_IN_FLIGHT; ++i )
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
        ulong frameCount = 0;
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

            float time  = std::chrono::duration<float>( t1 - startTime ).count();
            auto  model = Axion::Math::MTX::identity();
            // model       = Axion::Math::rotate( model, time * 1.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
            // model       = Axion::Math::rotate( model, time * 0.5f, Math::Vec3( 1.0f, 0.0f, 0.0f ) );

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

                cpypass.inputHandle  = rtPass.output;
                cpypass.outputHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );
                builder.addPass( "CopyPass", cpypass );
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
    uint              id,
    uint              hitGroup,
    const Math::Vec3& pos,
    const Math::Vec3& scale,
    const Math::Vec3& color,
    ulong             blasAddress ) {
    Axion::Graphics::RHI::AccelInstanceDesc inst = {};
    inst.instanceID                              = id;
    inst.instanceMask                            = 0xFF;
    inst.hitGroupIndex                           = hitGroup;
    inst.blasDeviceAddress                       = blasAddress;

    // Matriz TRS (Sin rotación para simplificar cajas AABB aligned)
    auto m         = Axion::Math::MTX::identity();
    m              = Axion::Math::MTX::translate( m, pos );
    m              = Axion::Math::MTX::scale( m, scale );
    inst.transform = Axion::Math::MTX::transpose( m ); // Row-Major para DXR

    return inst;
}
