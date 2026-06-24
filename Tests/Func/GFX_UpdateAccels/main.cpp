
#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/IRenderer.h"
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "cube.h"

USING_AXION_NAMESPACE

struct Scene {
    Math::Vec3 camPos = { 0.0f, 0.0f, -3.0f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewProj;
        Math::Mat4 invView;
        Math::Mat4 invProj;
        Math::Mat4 model;
    };
};

struct Cube {

    Graphics::BufferHandle vbo;
    Graphics::BufferHandle ibo;
    Graphics::AccelHandle  accel;

    std::vector<Vertex> vertices = cubeVertices;
    std::vector<u32>    indices  = cubeIndices;
};

struct RTXPass {
    Graphics::PipelineHandle rtPipeline;

    Cube cubeData;

    Graphics::RGResourceHandle output;      // ColorBuffer
    Graphics::BufferHandle     uboHandle;   // Camera Uniform Buffer
    Graphics::AccelHandle      accelHandle; // TLAS

    Graphics::RHI::AccelInstanceDesc currInst;
    bool                             shouldUpdate = false;

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

        // Update Accel
        Graphics::RHI::AccelDesc newDesc = accel->getDescription();
        newDesc.instances                = { currInst };
        if ( shouldUpdate )
            ctx.cmd->updateAccel( accel, newDesc, *ctx.transAllocator );
        else
            ctx.cmd->buildAccel( accel, newDesc, *ctx.transAllocator );

        ctx.cmd->bindRaytracingPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, accel );
        set0->attach( 0, Graphics::RHI::DescriptorType::UAV_Image, targetTex );
        set0->attach( 0, Graphics::RHI::DescriptorType::CBV, ubo );
        ctx.cmd->bindDescriptorSet( 0, set0 );

        Graphics::RHI::SBT sbt;
        sbt.setRayGen( "raygenMain" );
        sbt.addMiss( "missMain" );
        sbt.addHitGroup( "HitGroup" );

        auto sbtView = ctx.allocateSBT( sbt, pso );
        ctx.cmd->dispatchRays( sbtView, targetTex->getDescription().size );
    }
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "GFXAccelUpdateTest.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX Accel Update TEST" } );

        auto      bufferingType    = Graphics::BufferingType::Double;
        const u32 FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto      rnd              = Axion::Graphics::createRenderer( wnd.get(),
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
            .path( AXION_TESTS_RESOURCE_DIR "/Raytracing.slang" )
            .raygen( "raygenMain" )
            .miss( "missMain" )
            .closestHit( "hitMain" )
            .load();

        rnd->shaders().compileAllShaders();

        RTXPass rtPass {};
        rtPass.rtPipeline = rnd->pipelines()
                                .raytracing( "RTXPipeline" )
                                .shader( "RTXShader" )
                                .defineHitGroup( "HitGroup", "hitMain" )
                                .setMaxDepth( 1 )
                                .setPayloadSize( sizeof( Axion::Math::Vec4 ) )
                                .create();

        Axion::Graphics::Passes::BlitToBackBuffer cpypass {};

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

        auto keyEvent = wnd->onKey().subscribe( [&]( const Event::KeyEvent& e ) {
            if ( e.keyCode == Event::KeyCode::W && e.pressed )
            {
                rtPass.shouldUpdate = !rtPass.shouldUpdate;
            }
        } );

        // 2. TLAS (Top Level Acceleration Structure)

        auto*                                                cubeBlas = rnd->resources().getAccel( rtPass.cubeData.accel );
        std::vector<Axion::Graphics::RHI::AccelInstanceDesc> instances;

        Axion::Graphics::RHI::AccelInstanceDesc inst = {};
        inst.instanceID                              = 0;
        inst.instanceMask                            = 0xFF;
        inst.hitGroupIndex                           = 0;
        inst.blasDeviceAddress                       = cubeBlas->getDeviceAddress();

        auto m         = Axion::Math::MTX::identity();
        inst.transform = Axion::Math::MTX::transpose( m ); // Row-Major DXR

        rtPass.accelHandle = rnd->resources()
                                 .accel( "TLAS" )
                                 .asTLAS()
                                 .intances( { inst } )
                                 .allowUpdate()
                                 //  .instantBuild()
                                 .create();

        // UNIFORM CONSTANT BUFFER
        std::vector<Graphics::BufferHandle>
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
            auto  proj   = Axion::Math::MTX::perspective( Math::radians( scn.fov ), aspect, 0.01f, 10.0f );

            auto view = Axion::Math::MTX::lookAt( scn.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            float time  = std::chrono::duration<float>( t1 - startTime ).count();
            auto  model = Axion::Math::MTX::identity();
            model       = Axion::Math::MTX::rotate( model, time * 1.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
            model       = Axion::Math::MTX::rotate( model, time * 0.5f, Math::Vec3( 1.0f, 0.0f, 0.0f ) );

            Scene::Payload payload;
            payload.viewProj = proj * view;
            payload.viewProj = Axion::Math::MTX::transpose( payload.viewProj );
            payload.invView  = Axion::Math::MTX::inverse( Axion::Math::MTX::transpose( view ) );
            payload.invProj  = Axion::Math::MTX::inverse( Axion::Math::MTX::transpose( proj ) );
            payload.model    = Axion::Math::MTX::transpose( model );

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

                rtPass.uboHandle          = scnBuffers[frameIndex];
                rtPass.currInst           = inst;
                rtPass.currInst.transform = payload.model;

                builder.addPass<RTXPass>( "RTPass", rtPass );

                cpypass.inputHandle  = rtPass.output;
                cpypass.outputHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );
                builder.addPass( "CopyPass", cpypass );
            } );

            if ( time > 10.0f )
                break;
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
