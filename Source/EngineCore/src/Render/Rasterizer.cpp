
#pragma once
#include "GPUScene.h"
#include <Axion/Core/Render/Rasterizer.h>
#include <Axion/Graphics/Renderer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class Rasterizer final : public IRasterizer
{
public:
    ~Rasterizer();

    bool compileShaders( uint threadCount = 0, const std::string filePath = {} ) override;
    void render( const Scene::Scene& scene, Scene::Entity& cameraEntity ) override;
    void shutdown() override;

    MemoryBudget   getMemoryBudget() const override;
    CommonSettings getCommonSettings() const override;

    Settings getSettings() const override;
    void     setSettings( const Settings& settings ) override;

    void ulong getCurrentFrameIndex() const override;
    void ulong getTotalFrameNumber() const override;
    const uint getTotalFramesInFlight() const override { return _rnd->getTotalFramesInFlight(); };

    std::string toString() const override;

    Rasterizer( Platform::Window* wnd, const RasterizerSettings& settings );

private:
    void createPipelines();
    void createResources();

    Platform::Window*  _window = nullptr;
    RasterizerSettings _settings;

    GPUScene _gpuScene;

    // Graphics

    // std::vector<MaterialArchetype> _archetypes;
    // std::unordered_map<std::string, uint> _archetypeLookup;

    struct GPUResources {
        Graphics::BufferHandle         geomBufferHandle;
        Graphics::RHI::LinearAllocator geomAllocator;
        Graphics::BufferHandle         matBufferHandle;
        Graphics::RHI::LinearAllocator matAllocator;

        std::vector<Graphics::BufferHandle>         uboBufferHandles;    // Per-frame
        std::vector<Graphics::RHI::LinearAllocator> uboBufferAllocators; // Per-frame

        Graphics::AccelHandle staticTLASHandle;
        Graphics::AccelHandle dynamicTLASHandle;
    };

    Graphics::RendererPtr _rnd = nullptr;
    GPUResources          _res;

    // Render Passes

    struct RenderPasses {

        std::vector<RenderPassPtr> preprocess;

        // Main
        RenderPassPtr voxelization = nullptr;
        RenderPassPtr geometry     = nullptr;
        RenderPassPtr composition  = nullptr;
        RnderPassPtr  forward      = nullptr; // For transmissive

        // Especials
        RenderPassPtr hairLUT           = nullptr;
        RenderPassPtr hairVoxelization  = nullptr;
        RenderPassPtr panoramaToCubeMap = nullptr;
        RenderPassPtr computeIrradiance = nullptr;
        RenderPassPtr sky               = nullptr;

        std::vector<RenderPassPtr> postprocess; // tonemapping, fxaa, gui etc
    };

    RenderPasses _passes;

    uint _framesInFlight;
};

Rasterizer::Rasterizer( Platform::Window* wnd, const RasterizerSettings& settings )
    : _window( wnd )
    , _settings( settings ) {

    AXION_LOG_ASSERT( wnd, Logger::Module::Core, "Window is null" );

    Graphics::RendererSettings rndStts {
        .gfxApi                = settings.gfxApi;
        .bufferingType         = settings.bufferingType;
        .debugMode             = settings.debugMode;
        .presentMode           = wnd->getSettings().vsync ? Graphics::PresentMode::Vsync : PresentMode::Immediate;
        .backbufferFormat      = settings.backbufferFormat;
        .RGAllocSize           = settings.memory.RGAllocSize;
        .RGAllocSBTSize        = settings.memory.RGAllocSBTSize;
        .RGDescriptorsPerFrame = settings.memory.RGDescriptorsPerFrame;
        .RGTransientAllocSize  = settings.memory.RGTransientAllocSize;
        ;
        .GCMode   = settings.GCMode;
        .autoSync = true; // Use RG automatic barrier resolver
    };

    _rnd = Graphics::createRenderer( wnd->getNativeWindow(), rndStts );

    createResources();

    // Declare passes using the PassManager
    //  registerPass<ToneMapping>()
    //  ...

    AXION_LOG_INFO( Logger::Module::Core, "Renderer [{}] Created Succesfully", _settings.common.name );
}

Rasterizer::~Rasterizer() {
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Renderer [{}]", _settings.common.name );
    shutdown();
}

void Rasterizer::render( const Scene::Scene& scene, Scene::Entity& cameraEntity ) {

    _gpuScene.update( scene, cameraEntity, _window->getExtent(), 0.0f );

    UploadPass upPass {};

    TrianglePass rpass {};
    rpass.pipelineHandle = _pipShaderMap[0];

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
        rpass.output       = builder.import( "Backbuffer", rnd->getCurrentBackbufferHandle() );
        rpass.cameraBuffer = camBuffers[frameIndex];

        builder.addPass<TrianglePass>( "TrianglePass", rpass );
    } );
}

void Rasterizer::createResources() {

    auto& r = _rnd->resources(); 

    // A. GLOBAL BUFFERS (Persistent)
    _res.geomBufferHandle = r.buffer( "GlobalGeometryBuffer" ).size( _settings.memory.geometryBufferSize ).asRaw().create();
    _res.matBufferHandle  = r.buffer( "GlobalMaterialBuffer" ).size( _settings.memory.materialBufferSize ).asRaw().create();

    // NOTA: Recuerda cambiar esto a FreeListAllocator/BuddyAllocator en el futuro
    _res.geomAllocator = Graphics::RHI::LinearAllocator( r.getBuffer( _res.geomBufferHandle ) );
    _res.matAllocator  = Graphics::RHI::LinearAllocator( r.getBuffer( _res.matBufferHandle ) );

    // B. PER-FRAME BUFFERS (Volatile)
    _framesInFlight = _rnd->getTotalFramesInFlight();
    _res.uboBufferHandles.resize( _framesInFlight );
    _res.uboBufferAllocators.reserve( _framesInFlight ); 

    for ( uint i = 0; i < _framesInFlight; ++i )
    {
        _res.uboBufferHandles[i] = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                       .size( _settings.memory.volatileBufferSize )
                                       .asUpload() // CPU Write / GPU Read
                                       .create();

        _res.uboBufferAllocators.emplace_back( r.getBuffer( _res.uboBufferHandles[i] ) );
    }
}

void Rasterizer::createPipelines() { /* ... */ }

bool Rasterizer::compileShaders()( uint threadCount = 0, const std::string filePath = {} ) {

    bool sucess = false;

    // In the future we could reand the file p xml/python and define materials from it.
    // For now, we manually declare

    // First materials
    registerMaterialType( "StandardPBR", AXION_RESOURCES_PATH "/Shadesrs/StandardPBR.slang" )

        // Then the render passes

        for ( aunto& pass : _passes.preprocess ) {
        pass.registerShader();
    }

    _passes.voxelization.registerShader()();
    _passes.geometry.registerShader()();
    _passes.composition.registerShader()();
    _passes.forward.registerShader()();

    _passes.hairLUT.registerShader()();
    _passes.hairVoxelization.registerShader()();
    _passes.panoramaToCubeMap.registerShader()();
    _passes.computeIrradiance.registerShader()();
    _passes.sky.registerShader()();

    for ( aunto& pass : _passes.postprocess )
    {
        pass.registerShader()();
    }

    success = _rnd->shaders().compileAllShaders( threadCount );

    //------------------------
    // Here create pipelines
    createMaterialPipelines( "StandardPPBR" );

    for ( aunto& pass : _passes.preprocess )
    {
        pass.createPipeline();
    }

    _passes.voxelization.createPipeline()();
    _passes.geometry.createPipeline()();
    _passes.composition.createPipeline()();
    _passes.forward.createPipeline()();

    _passes.hairLUT.createPipeline()();
    _passes.hairVoxelization.createPipeline()();
    _passes.panoramaToCubeMap.createPipeline()();
    _passes.computeIrradiance.createPipeline()();
    _passes.sky.createPipeline()();

    for ( aunto& pass : _passes.postprocess )
    {
        pass.createPipeline()();
    }

    return sucess;
}

void Rasterizer::shutdown() { /* Liberar cosas */ }

void Rasterizer::updateResources() { /* Consumir colas de GPUScene */ }

// void Rasterizer::registerMaterialType(const std::string& name, const std::string& shaderPath) {
//     if (_archetypeLookup.contains(name)) return;

//     MaterialArchetype arch;
//     // 1. Compile Shader
//     arch.shader = compile(shaderPath);

//

//     _archetypes.push_back(arch);
//     _archetypeLookup[name] = _archetypes.size() - 1;
// }
// void Rasterizer::rcreateMaterialPipelines(const std::string& nameh) {
//
//     // 2. Create Variants (The "50 shaders x 4 types" solution)
//     // You automate this! Don't write it manually 50 times.
//     arch.pipelines[Forward] = createPipeline(arch.shader, Blend::Opaque, Depth::Write);
//     arch.pipelines[Shadow]  = createPipeline(arch.shader, Blend::Off, ColorWrite::Off);
//     // ...

//
// }

// Factory
RendererPtr createRasterizer( Platform::Window* wnd, const RasterizerSettings& settings ) {
    return NEW_U( Rasterizer )( wnd, settings );
}

} // namespace Core::Render

AXION_NAMESPACE_END