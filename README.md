<div align="center">

# AXION ENGINE ⚡
### DirectX 12 & Vulkan Agnostic High Performance Render Framework
*Tailored for Experimentation & Academy*

[Documentation](https://aespinosadev.github.io/AxionEngine/) | [Features](#key-features-) | [Building](#building-)



<br>

> 🚀 **Latest Update:** GFX Module is now `Production Ready`.

<br>

<img width="75%" alt="Screenshot (253)" src="https://github.com/user-attachments/assets/6ae452b5-773d-42bf-a478-3de6bc0cd6f8" />
<img width="75%" alt="image" src="https://github.com/user-attachments/assets/0abed45a-cfde-492f-afca-3f412a55e80e" />

<p><i> Top: Raster / Bottom: Real-Time Path-Tracing</i></p>
</div>

## Modular Architecture 🧩

Axion is designed with strict modularity in mind. You are not forced to use the entire engine stack; you can pick and choose modules based on your prototyping needs.

- **`AxionCommon`**: Shared utilities, math libraries, and base types.
- **`AxionGFX`**: High-level, API-agnostic rendering framework (RHI + RenderGraph). *Can be used standalone.*
- **`AxionCore`**: Scene management, Asset Loading, and high-level logic.
- **`AxionEditor`**: The sandbox environment and tooling.

### Prototyping Workflows
* **Pure Graphics:** Use `AxionGFX` alone for low-level graphics experiments (Compute Shaders, Raytracing) without the overhead of a game engine scene graph.
* **Interop (Fast Prototyping):** Use `AxionCore` solely as a resource loader (AssetManager) to feed data into `AxionGFX`, bypassing the ECS/Scene systems entirely.
* **Render Engine (WIP):** Use `AxionCore` for creating fast and efficient graphic applications tailored for any need.
* **Designer (WIP):** Use `AxionEditor` for loading entire designed scenes and rendering them with beautiful graphics. Add interactivity making use of the SceneGraph components.

## Key Features ✨

- **Advanced RenderGraph:** Automatic barrier insertion, transient resource management, and memory aliasing.
- **Declarative API:** Fluent Builder pattern for defining pipelines and resources easily.
- **Multi-Pipeline Support:** Robust support for Compute, Graphics, and Ray Tracing (WIP).
- **Shader System:**
    - Hot-Reloading support.
    - **Automatic Reflection** using SLANG.
    - Agnostic compilation to DXIL and SPIR-V.
- **Modern Architecture:** PIMPL idioms, ECS integration, and strict RAII resource management.
- **Tooling:** Integrated Logger, Windowing, and Event systems.

*This project is a work in progress.*

## Building 🛠️

### Prerequisites

- **OS:** Windows 10/11.
- **SDKs:** Vulkan SDK 1.4.*.
- **Tools:** CMake (3.20+), Ninja 🥷 (Optional, recommended for speed).´
- **Git LFS:** Required to download binary assets (textures, meshes). 
    *(If you clone the repo and assets appear as 1KB text files, run `git lfs pull`)*.

`Heavy dependencies that need to be built (GLFW, fmt, etc) will be fetch recursively. Slang and other necessary binaries will be automatically downloaded using Cmake's fetch content`

### Steps

1. **Clone the repository:**
   ```bash
   git clone --recursive [https://github.com/AEspinosaDev/AxionEngine.git](https://github.com/AEspinosaDev/AxionEngine.git)
   cd AxionEngine
   ```
   
2. **Build with CMake:**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

   Note: The CMake configuration automatically locates and links system dependencies (except Vulkan SDK). It is designed to work out-of-the-box with VS Code or Visual Studio.
   
3. **Options: To disable building tests, examples or specific engine modules:**
   ```bash
    cmake -DAXION_BUILD_EDITOR=OFF .. 
    cmake -DAXION_BUILD_CORE=OFF ..  
    cmake -DAXION_BUILD_GFX=OFF ..  

    cmake -DAXION_BUILD_TESTS=OFF ..  
    cmake -DAXION_BUILD_SAMPLES=OFF ..
   ```

## Usage Example 🚀

### Raster Pipeline

This example demonstrates how to set up a complete Raster Pipeline that generates the typical triangle.

Notice how Resource Barriers, Descriptor Sets, and Layouts are handled implicitly by the engine's RenderGraph and Reflection systems.

In less than 200 lines of code you have a complete rasterization framework built on top of DX12/Vulkan, Uniform Buffers and Geometry running.

 ```cpp
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
        Axion::Logger::init( Logger::Level::Info, "Engine.log" );
#endif

        auto wnd = Axion::Graphics::createWindowForWin32( GetModuleHandle( nullptr ), { .name = "GFX RASTER TEST" } );

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

        rnd->shaders().shader( "DrawShader" ).asDXIL().path( AXION_SHADER_DIR "/Slang/Testing/Raster.slang" ).vs( "vsMain" ).ps( "psMain" ).load();
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

        static auto startTime = std::chrono::high_resolution_clock::now();
        while ( !wnd->shouldClose() )
        {
        
            wnd->processMessages();

            // Process Uniforms

            float aspect = (float)wnd->getSettings().size.width / (float)wnd->getSettings().size.height;
            auto  proj   = Axion::Math::perspective( Math::radians( cam.fov ), aspect, 0.01f, 10.0f );
            auto  view   = Axion::Math::lookAt( cam.camPos, { 0, 0, 0 }, { 0, 1, 0 } );

            Camera::Payload camData;
            camData.viewProj = proj * view;
            camData.viewProj = Axion::Math::transpose( camData.viewProj );

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

 ```

<div align="center"> <img src="https://github.com/user-attachments/assets/f43e7b55-b4a9-46df-899d-413e33621a5d" width="600" alt="Axion Engine Raster Output"> <p><i>Raster Shader output running on DX12 backend.</i></p> </div>

### Compute Pipeline

This example demonstrates how to set up a complete Compute Pipeline that generates an HDR image, applies Tone Mapping, and blits the result to the Backbuffer.

Notice how Resource Barriers, Descriptor Sets, and Layouts are handled implicitly by the engine's RenderGraph and Reflection systems.

 ```cpp
#include "Axion/Graphics/Renderer.h"
#include "Axion/Graphics/Platforms/Win32.h"

USING_AXION_NAMESPACE

// 1. Define your Passes

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
        set0->bind( 0, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

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
        auto* set1 = ctx.allocateSet( pso->getDescription().layout, 1 );
        set0->bind( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
        set1->bind( 0, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

        ctx.cmd->bindComputePipeline( pso );
        ctx.cmd->bindDescriptorSet( 0, set0 );
        ctx.cmd->bindDescriptorSet( 1, set1 );

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

int main() {
    try {
        // Init Subsystems
        auto wnd = Axion::Graphics::createWindowForWin32(GetModuleHandle(nullptr), { .name = "AXION DEMO" });
        auto rnd = Axion::Graphics::createRenderer(wnd, { 
            .gfxApi = Graphics::API::DirectX12, 
            .presentMode = Graphics::PresentMode::Vsync 
        });

        // Load Shaders (Hot-Reloadable)
        rnd->shaders().shader("GenShader").asDXIL().path("Shaders/Gen.slang").cs("computeMain").load();
        rnd->shaders().shader("ToneMapShader").asDXIL().path("Shaders/ToneMap.slang").cs("computeMain").load();
        rnd->shaders().compileAllShaders();

        // Create Pipelines
        GenerationPass gpass;
        gpass.pipelineHandle = rnd->pipelines().compute( "GenerationPipeline" ).shader( "GenerationShader" ).create();
        ToneMappingPass tpass;
        tpass.pipelineHandle = rnd->pipelines().compute( "TonemappingPipeline" ).shader( "TonemappingShader" ).create();
        CopyPass cpypass;

        auto evnt = wnd->onKey().subscribe( [&gpass]( const Event::KeyEvent& e ) { 
            if ( e.keyCode == 38 && e.pressed ){
            gpass.pushData.speed += 0.1;
        }
            if ( e.keyCode == 40 && e.pressed ){
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



   ```


   <div align="center"> <img src="https://github.com/user-attachments/assets/d1af24cf-0474-418e-8d8c-f15346d6d697" width="600" alt="Axion Engine Compute Output"> <p><i>Compute Shader output with dynamic tone mapping running on DX12 backend.</i></p> </div>





