/*
 * ==========================================================================================
 * AXION ENGINE - MESH SHADING CUBE SAMPLE
 * ==========================================================================================
 * * Author:    Antonio J. Espinosa
 * Date:        2026
 *
 * Description:
 * Entry point for the Mesh Shading demonstration. This sample implements a render of a
 * textured mesh bypassing the traditional Input Assembler.
 *
 * It uses the Core::Assets module to procedurally generate a Meshlet-ready cube,
 * uploads the geometry as standard Byte Address Buffers (SSBOs/SRVs), and launches
 * a Mesh Pipeline via DispatchMesh.
 *
 * ==========================================================================================
 */
#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Graphics/Passes/PostProcess.hpp"
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "Axion/Graphics/IRenderer.h"

USING_AXION_NAMESPACE

struct Camera {
    Math::Vec3 camPos = { 0.0f, 0.0f, -2.0f };
    float      fov    = 60.0f;

    struct Payload {
        Math::Mat4 viewModelProj;
    };
};

struct MeshletModel {
    // We no longer use VBO/IBO. These are raw data buffers.
    Graphics::BufferHandle vertexBuffer;
    Graphics::BufferHandle meshletBuffer;
    Graphics::BufferHandle vertexIndexBuffer;
    Graphics::BufferHandle primitiveIndexBuffer;

    uint meshletCount = 0;

    Graphics::TextureHandle texture;
    Graphics::SamplerHandle sampler;
};

struct ForwardPass {
    Graphics::PipelineHandle pipeline;

    MeshletModel modelData = {};

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
        auto* pso = ctx.pipelines.getMeshPipeline( pipeline );

        auto* targetTex = ctx.getTexture( data.target );
        auto* depthTex  = ctx.getTexture( data.depthTarget );

        // Fetch resources
        auto* ubo     = ctx.resources.getBuffer( cameraBuffer );
        auto* sampler = ctx.resources.getSampler( modelData.sampler );
        auto* texture = ctx.resources.getTexture( modelData.texture );

        auto* vb  = ctx.resources.getBuffer( modelData.vertexBuffer );
        auto* mb  = ctx.resources.getBuffer( modelData.meshletBuffer );
        auto* vib = ctx.resources.getBuffer( modelData.vertexIndexBuffer );
        auto* pib = ctx.resources.getBuffer( modelData.primitiveIndexBuffer );

        Graphics::RHI::RenderingDesc info;
        info.renderArea = targetTex->getDescription().size.to2D();
        info.colorAttachments.push_back( { .texture = targetTex } );
        info.depthStencilAttachment = { .texture = depthTex };

        ctx.cmd->beginRendering( info );

        ctx.cmd->bindMeshPipeline( pso );

        auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );

        // Bind Standard Resources
        set0->attach( 0, ubo, Graphics::RHI::ResourceState::ConstantBuffer );
        set0->attach( 1, texture, Graphics::RHI::ResourceState::ShaderResource );

        // Bind Geometry Data as Shader Resources (SRVs / ByteAddressBuffers)
        set0->attach( 2, vb, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 3, mb, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 4, vib, Graphics::RHI::ResourceState::ShaderResource );
        set0->attach( 5, pib, Graphics::RHI::ResourceState::ShaderResource );

        set0->attach( 0, sampler );

        ctx.cmd->bindDescriptorSet( 0, set0 );

        ctx.cmd->dispatchMesh( { modelData.meshletCount, 1, 1 } );

        ctx.cmd->endRendering();
    }
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Axion::Logger::init( Logger::Level::Info, "MeshShadingSample.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "MESH SHADING SAMPLE" } );

        auto       bufferingType    = Graphics::BufferingType::Double;
        const uint FRAMES_IN_FLIGHT = (size_t)bufferingType + 1;
        auto       rnd              = Axion::Graphics::createRenderer( wnd.get(),
                                                                       { .gfxApi        = Graphics::API::DirectX12,
                                                                         .bufferingType = bufferingType,
                                                                         .presentMode   = Graphics::PresentMode::Immediate,
                                                                         .autoSync      = true,
                                                                         .enableGui     = true } );

        Axion::Core::Assets::AssetManager assets;

        //-------------------------------------
        // Declaring Shaders & Pipelines
        //-------------------------------------

        rnd->shaders()
            .shader( "MeshDrawShader" )
            .asDXIL()
            .path( AXION_SAMPLES_RESOURCE_DIR "/Shaders/MeshShading.slang" )
            .ms( "msMain" ) // Mesh Shader entry point
            .ps( "psMain" ) // Pixel Shader entry point
            .load();

        rnd->shaders().compileAllShaders();

        Axion::Graphics::Passes::BlitToBackBuffer cpypass {};
        Axion::Graphics::Passes::PresentPass      presentpass {};
        Axion::Graphics::Passes::ToneMapping      tmPass {};

        tmPass.init( *rnd.get() );

        ForwardPass fwPass {};

        // Use the new MeshBuilder from the registry
        fwPass.pipeline = rnd->pipelines()
                              .mesh( "FwPipeline" )
                              .shader( "MeshDrawShader" )
                              .addRenderTarget( Axion::Graphics::Format::RGBA16_FLOAT )
                              .setDepthFormat( Graphics::Format::D32 )
                              .cullNone()
                              .create();

        //-------------------------------------
        // Declaring Static Resources
        //-------------------------------------

        auto  cpuTexHandle = assets.texture( "DebugTexture" ).import( AXION_MESH_DIR "/erato/erato-101.jpg" );
        auto* cpuTexture   = assets.getTexture( cpuTexHandle );

        if ( !cpuTexture )
            return EXIT_FAILURE;

        fwPass.modelData.texture = rnd->resources()
                                       .texture( "DebugTexture" )
                                       .format( cpuTexture->getGPUFormat() )
                                       .extent( cpuTexture->getSize() )
                                       .withData( cpuTexture->getPixels() )
                                       .create();

        fwPass.modelData.sampler = rnd->resources().sampler( "LinearSampler" ).create();

        // GEOMETRY (Procedural Meshlets)
        auto  cubeHandle = assets.mesh( "MeshletErato" ).asMeshlet( true ).import( AXION_MESH_DIR "/erato/erato.obj" );
        auto* cubeMesh   = assets.getMesh( cubeHandle );
        auto  geoData    = cubeMesh->getGeometryDataRef();

        fwPass.modelData.meshletCount = geoData->meshlets->meshlets.size();

        // Allocate Geometry Buffers as Storage Buffers (SRVs)
        fwPass.modelData.vertexBuffer = rnd->resources()
                                            .buffer( "VertexBuffer" )
                                            .asReadOnlySSBO() // Storage Buffer / SRV
                                            .withData( geoData->vertices.data() )
                                            .size( geoData->vertices.size() * sizeof( Core::Assets::Vertex ) )
                                            .stride( sizeof( Core::Assets::Vertex ) )
                                            .create();

        fwPass.modelData.meshletBuffer = rnd->resources()
                                             .buffer( "MeshletBuffer" )
                                             .asReadOnlySSBO()
                                             .withData( geoData->meshlets->meshlets.data() )
                                             .size( geoData->meshlets->meshlets.size() * sizeof( Core::Assets::Meshlet ) )
                                             .stride( sizeof( Core::Assets::Meshlet ) )
                                             .create();

        fwPass.modelData.vertexIndexBuffer = rnd->resources()
                                                 .buffer( "VertexIndexBuffer" )
                                                 .asReadOnlySSBO()
                                                 .withData( geoData->meshlets->vertexIndices.data() )
                                                 .size( geoData->meshlets->vertexIndices.size() * sizeof( uint ) )
                                                 .stride( sizeof( uint ) )
                                                 .create();

        fwPass.modelData.primitiveIndexBuffer = rnd->resources()
                                                    .buffer( "PrimitiveIndexBuffer" )
                                                    .asReadOnlySSBO()
                                                    .asRaw()
                                                    .withData( geoData->meshlets->primitiveIndices.data() )
                                                    .size( geoData->meshlets->primitiveIndices.size() * sizeof( uchar ) )
                                                    .create();

        // UNIFORM CONSTANT BUFFER
        std::vector<Graphics::BufferHandle> camBuffers( FRAMES_IN_FLIGHT );
        for ( uint i = 0; i < FRAMES_IN_FLIGHT; ++i )
        {
            camBuffers[i] = rnd->resources().buffer( "CamUniformBuffer_" + std::to_string( i ) ).size( sizeof( Camera::Payload ) ).asCBO().onCPU().create();
        }

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

            // Camera Math
            float aspect = (float)wnd->getSettings().size.width / (float)wnd->getSettings().size.height;
            auto  proj   = Axion::Math::MTX::perspective( Math::radians( cam.fov ), aspect, 0.01f, 10.0f );
            auto  view   = Axion::Math::MTX::lookAt( cam.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            float time  = std::chrono::duration<float>( std::chrono::high_resolution_clock::now() - startTime ).count();
            auto  model = Axion::Math::MTX::identity();
            model       = Axion::Math::MTX::rotate( model, time * 0.5f, Math::Vec3( 0.0f, 1.0f, 0.0f ) );
            model       = Axion::Math::MTX::translate( model, Math::Vec3( 0.0f, -0.8f, 0.0f ) );
            model       = Axion::Math::MTX::scale( model, Math::Vec3( 0.05f ) );

            Camera::Payload camData;
            camData.viewModelProj = proj * view * model;
            camData.viewModelProj = Axion::Math::MTX::transpose( camData.viewModelProj );

            auto  frameIndex = rnd->getCurrentFrameIndex();
            auto* cbRaw      = rnd->resources().getBuffer( camBuffers[frameIndex] );
            cbRaw->copyData( camData );

            rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
                using namespace Axion::Graphics;

                auto rtExtent = wnd->getSettings().size.to3D();

                fwPass.output = builder.texture( "ColorBuffer" )
                                    .asRenderTarget()
                                    .asStorage()
                                    .format( Format::RGBA16_FLOAT )
                                    .extent( rtExtent )
                                    .clearValue( { .color = { 0.2f, 0.2f, 0.2f, 1.0f } } )
                                    .create();

                fwPass.depthOutput = builder.texture( "DepthBuffer" )
                                         .asDepthStencil()
                                         .format( Format::D32 )
                                         .extent( rtExtent )
                                         .create();

                fwPass.cameraBuffer = camBuffers[frameIndex];

                builder.addPass<ForwardPass>( "ForwardPass", fwPass );

                tmPass.inputHandle  = fwPass.output;
                tmPass.outputHandle = builder.texture( "LDRIntermidiate" ).format( Format::RGBA8_UNORM ).extent( rtExtent ).asStorage().create();

                builder.addPass( "TonemappingPass", tmPass );

                auto backbufferHandle = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );

                cpypass.inputHandle  = tmPass.outputHandle;
                cpypass.outputHandle = backbufferHandle;
                builder.addPass( "CopyPass", cpypass );

                presentpass.inoutHandle = backbufferHandle;
                builder.addPass( "PresentPass", presentpass );
            } );
        };

        assets.deleteTexture( cpuTexHandle );

    } catch ( const std::exception& e )
    {
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}