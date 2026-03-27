/*
 * ==========================================================================================
 * AXION ENGINE - GFX MODULE'S VERTEX PULLING SAMPLE
 * ==========================================================================================
 * * Author:    Antonio J. Espinosa
 * Date:        2025
 *
 * Description:
 * Entry point for the Vertex Pulling for rasterization usage demonstration. This sample implements 
 * global vertex and index buffer for all geometry in scene. 4 different geometries (same geometry for simplify) are streamed onto the global buffers
 * and their offsets are modified according to it. By the use of push constants the current mesh info is uploaded (offsets and model matrix).
 * The vertex shader doesn't have any input layout, as no VBO nor IBO are used. It directly pulls geometry data from these global buffers.
 * This pipeline is the most used on modern render engines.
 *
 *
 * ==========================================================================================
 */
#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "Axion/Graphics/IRenderer.h"

#include "cube.h"
USING_AXION_NAMESPACE

struct Camera {
    Math::Vec3 camPos = { 0.0f, 0.0f, -2.0f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewProj;
    };
};

struct Mesh {
    bool  loaded    = false;

    struct Payload {
        Math::Mat4 model;
        Math::Vec4 color;
        uint       meshOffset   = 0;
        uint       meshIdOffset = 0;
    };

    Payload payload {};

    std::vector<Vertex>
                      vertices = cubeVertices;
    std::vector<uint> indices  = cubeIndices;
};

struct UploadPass {

    std::vector<Mesh>& meshes;

    Graphics::RGResourceHandle vbHandle;
    Graphics::RGResourceHandle ibHandle;

    struct Data {
        Graphics::RGResourceHandle vb;
        Graphics::RGResourceHandle ib;
    };

    void setup( Graphics::RenderPassBuilder& pb, Data& data ) {
        data.vb = pb.write( vbHandle, Graphics::RHI::ResourceState::CopyDest );
        data.ib = pb.write( ibHandle, Graphics::RHI::ResourceState::CopyDest );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {

        auto* vb = ctx.getBuffer( data.vb );
        auto* ib = ctx.getBuffer( data.ib );

        static uint currentVtxOffset = 0;
        static uint currentIdxOffset = 0;

        for ( auto& mesh : meshes )
        {
            if ( !mesh.loaded )
            {
                mesh.payload.meshOffset   = currentVtxOffset;
                mesh.payload.meshIdOffset = currentIdxOffset;

                uint vtxSize = mesh.vertices.size() * sizeof( Vertex );
                uint idxSize = mesh.indices.size() * sizeof( uint );

                ctx.cmd->uploadBuffer( vb, mesh.vertices.data(), vtxSize, currentVtxOffset, ctx.transAllocator, Graphics::RHI::BarrierPolicy::None );
                ctx.cmd->uploadBuffer( ib, mesh.indices.data(), idxSize, currentIdxOffset, ctx.transAllocator, Graphics::RHI::BarrierPolicy::None );

                currentVtxOffset += vtxSize;
                currentIdxOffset += idxSize;

                mesh.loaded = true;
            }
        }
    }
};

struct ForwardPass {
    Graphics::PipelineHandle pipeline;

    std::vector<Mesh>& meshes;

    Graphics::RGResourceHandle output;      // ColorBuffer
    Graphics::RGResourceHandle depthOutput; // DepthBuffer
    Graphics::RGResourceHandle vbHandle;
    Graphics::RGResourceHandle ibHandle;

    Graphics::BufferHandle cameraBuffer; // Camera Uniform Buffer

    struct Data {
        Graphics::RGResourceHandle target;
        Graphics::RGResourceHandle depthTarget;
        Graphics::RGResourceHandle vb;
        Graphics::RGResourceHandle ib;
    };

    void setup( Graphics::RenderPassBuilder& pb, Data& data ) {
        data.target      = pb.write( output, Graphics::RHI::ResourceState::RenderTarget );
        data.depthTarget = pb.write( depthOutput, Graphics::RHI::ResourceState::DepthWrite );
        data.vb          = pb.read( vbHandle, Graphics::RHI::ResourceState::GeneralRead );
        data.ib          = pb.read( ibHandle, Graphics::RHI::ResourceState::GeneralRead );
    }

    void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
        // PSO
        auto* pso = ctx.pipelines.getGraphicPipeline( pipeline );

        // RTs
        auto* targetTex = ctx.getTexture( data.target );
        auto* depthTex  = ctx.getTexture( data.depthTarget );

        // Cube Related
        auto* vb  = ctx.getBuffer( data.vb );
        auto* ib  = ctx.getBuffer( data.ib );
        auto* ubo = ctx.resources.getBuffer( cameraBuffer );

        Graphics::RHI::RenderingDesc info;

        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = targetTex } );
        info.depthStencilAttachment = { .texture = depthTex };

        ctx.cmd->beginRendering( info );
        ctx.cmd->bindGraphicPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
        set0->attach( 0, vb, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 1, ib, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 2, ubo, Graphics::RHI::ResourceState::ConstantBuffer );

        ctx.cmd->bindDescriptorSet( 0, set0 );

        for ( auto& mesh : meshes )
        {
            if ( mesh.loaded )
            {
                ctx.cmd->pushConstants( 1, mesh.payload );
                ctx.cmd->draw( (uint)mesh.indices.size() );
            }
        }

        ctx.cmd->endRendering();
    }
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "GFXVertexPullingSample.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX VERTEX PULLING SAMPLE" } );

        auto       bufferingType    = Graphics::BufferingType::Double;
        const uint FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto       rnd              = Axion::Graphics::createRenderer( wnd.get(),
                                                                       { .gfxApi        = Graphics::API::DirectX12,
                                                                         .bufferingType = bufferingType,
                                                                         .presentMode   = Graphics::PresentMode::Immediate,
                                                                         .autoSync      = true } );

        std::vector<Mesh> meshes;
        meshes.resize( 4, Mesh {} );
        std::vector<Math::Vec3> meshPositions = {
            Math::Vec3( 0.5f, -0.5f, 0.0f ),
            Math::Vec3( -0.5f, -0.5f, 0.0f ),
            Math::Vec3( 0.5f, 0.5f, 0.0f ),
            Math::Vec3( -0.5f, 0.5f, 0.0f ) };
        std::vector<Math::Vec4> meshColors = {
            Math::Vec4( 1.0f, 0.0f, 0.0f, 1.0f ),
            Math::Vec4( 0.0f, 1.0f, 0.0f, 1.0f ),
            Math::Vec4( 1.0f, 0.0f, 1.0f, 1.0f ),
            Math::Vec4( 0.0f, 0.0f, 1.0f, 1.0f ) };

        //-------------------------------------
        // Declaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders()
            .shader( "DrawShader" )
            .asDXIL()
            .path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/VertexPulling.slang" )
            .include( AXION_SHADER_DIR "/Slang/Common" )
            .vs( "vsMain" )
            .ps( "psMain" )
            .load();

        rnd->shaders().compileAllShaders();

        UploadPass  uploadPass { .meshes = meshes };
        ForwardPass fwPass { .meshes = meshes };
        fwPass.pipeline = rnd->pipelines()
                              .graphic( "FwPipeline" )
                              .shader( "DrawShader" )
                              .addRenderTarget( Axion::Graphics::Format::RGBA8_UNORM ) //  Format
                              .setDepthFormat( Graphics::Format::D32 )                 // Depth Format
                              .cullNone()                                              // Enable culling later if needed
                              .create();

        Axion::Graphics::Passes::BlitToBackBuffer cpypass {};
        Axion::Graphics::Passes::PresentPass      presentpass {};


        //-------------------------------------
        // Dedclaring Global Persistent Resources
        //-------------------------------------

        // GEOMETRY GLOBAL BUFFERS

        auto vboHandle = rnd->resources()
                             .buffer( "VertexBuffer" )
                             .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                             .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                             .size( 1024 * 1024 * 64 )
                             .onGPU()
                             .asRaw()
                             .create();

        auto iboHandle = rnd->resources()
                             .buffer( "IndexBuffer" )
                             .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                             .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                             .size( 1024 * 1024 * 64 )
                             .onGPU()
                             .asRaw()
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
            auto  proj   = Axion::Math::MTX::perspective( Math::radians( cam.fov ), aspect, 0.01f, 10.0f );

            auto view = Axion::Math::MTX::lookAt( cam.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            float time = std::chrono::duration<float>( t1 - startTime ).count();

            Camera::Payload camData;
            camData.viewProj = proj * view;
            camData.viewProj = Axion::Math::MTX::transpose( camData.viewProj );

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( camBuffers[frameIndex] );
            cbRaw->copyData( camData );

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto rtExtent = wnd->getSettings().size.to3D();

                // Update geometries:
                bool needUpload = false;
                uint meshId     = 0;
                for ( auto& mesh : meshes )
                {

                    if ( !mesh.loaded )
                    {
                        needUpload = true;
                    }
                    auto model = Axion::Math::MTX::identity();
                     model      = Axion::Math::MTX::translate( model, meshPositions[meshId] );
                    model = Axion::Math::MTX::scale( model, 0.5 );
                    model = Axion::Math::MTX::rotate( model, time * 1.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
                    model = Axion::Math::MTX::rotate( model, time * 0.5f, Math::Vec3( 1.0f, 0.0f, 0.0f ) );
                    model = Axion::Math::MTX::transpose( model );


                    mesh.payload.model = model;
                    mesh.payload.color = meshColors[meshId];

                    meshId++;
                }

                auto vbRg           = builder.import( "VertexBuffer", vboHandle );
                auto ibRg           = builder.import( "IndexBuffer", iboHandle );
                uploadPass.vbHandle = vbRg;
                uploadPass.ibHandle = ibRg;
                fwPass.vbHandle     = vbRg;
                fwPass.ibHandle     = ibRg;

                if ( needUpload )
                    builder.addPass<UploadPass>( "UploadPass", uploadPass );

                // 1. HDR Color Buffer (Transient)
                fwPass.output = builder.texture( "ColorBuffer" )
                                    .asRenderTarget()
                                    .asStorage() // Allow reading as SRV in next pass
                                    .format( Format::RGBA8_UNORM )
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

              
                 auto backbufferHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );

                cpypass.inputHandle  = fwPass.output;
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
