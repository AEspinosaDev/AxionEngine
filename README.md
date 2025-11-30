<div align="center">

# AXION ENGINE ⚡
### DirectX 12 & Vulkan Agnostic High Performance Render Framework
*Tailored for Experimentation & Academy*

[Documentation](https://aespinosadev.github.io/AxionEngine/) | [Features](#key-features-) | [Building](#building-)

</div>

<br>

## Engine Structure 🗃️

Axion is built with a modular design philosophy:

- **Common Module:** Shared utilities and base types.
- **Graphics Module:** High-level rendering abstraction (includes the **RHI** submodule).
- **Core Module:** Scene management and high-level logic.
- **Editor App:** The sandbox environment.

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
- **SDKs:** Vulkan SDK 1.4.* (Must include **SLANG**).
- **Tools:** CMake (3.20+), Ninja 🥷 (Optional, recommended for speed).

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
   
3. **Options: To disable building tests:**
   ```bash
    cmake -DAXION_ENABLE_TESTS=OFF ..
   ```

## Usage Example 🚀

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
