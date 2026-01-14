
#include "Rasterizer.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Factory
RendererPtr createRasterizer( Platform::Window* wnd, const RasterizerSettings& settings ) {
    return NEW_U( Rasterizer )( wnd, settings );
}

Rasterizer::Rasterizer( Platform::Window* wnd, const RasterizerSettings& settings )
    : _window( wnd )
    , _settings( settings ) {

    AXION_LOG_ASSERT( wnd, Logger::Module::Core, "Window is null" );

    Graphics::RendererSettings rndStts = {
        .gfxApi                = settings.common.gfxApi,
        .bufferingType         = settings.common.bufferingType,
        .debugMode             = settings.common.debugMode,
        .presentMode           = wnd->getSettings().vsync ? Graphics::PresentMode::Vsync : Graphics::PresentMode::Immediate,
        .backbufferFormat      = settings.common.backbufferFormat,
        .RGAllocSize           = settings.memory.RGAllocSize,
        .RGAllocSBTSize        = settings.memory.RGAllocSBTSize,
        .RGDescriptorsPerFrame = settings.memory.RGDescriptorsPerFrame,
        .RGTransientAllocSize  = settings.memory.volatileBufferSize,
        .GCMode                = settings.common.GCMode,
        .autoSync              = true,
        .selectedDeviceID      = settings.common.selectedDeviceID };

    _rnd = Graphics::createRenderer( wnd->getNativeWindow(), rndStts );

    // Configure Material Library & Contract
    setupMaterialLibrary();

    // Registrations
    registerMaterials();
    registerPasses();

    // GPU Resurce Creation
    createResources();

    AXION_LOG_INFO( Logger::Module::Core, "Renderer [{}] Created Succesfully", _settings.common.name );
} // namespace Core::Render

Rasterizer::~Rasterizer() {
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Renderer [{}]", _settings.common.name );
    shutdown();
}

void Rasterizer::shutdown() {}

ulong Rasterizer::getCurrentFrameIndex() const {
    return _rnd->getCurrentFrameIndex();
}

ulong Rasterizer::getTotalFrameNumber() const {
    return _rnd->getTotalFrameNumber();
}

std::string Rasterizer::toString() const {
    return std::string();
}

void Rasterizer::compileShaders( uint threadCount ) {

    auto startTime = std::chrono::high_resolution_clock::now();
    AXION_LOG_INFO( Logger::Module::Core, "Start of shader compilation for Renderer [{}] | Num Threads: {}", _settings.common.name, threadCount );

    // Register Material & Pass Shaders
    _mtlLib.registerShaders( _rnd->shaders() );
    _passes.registerShaders( _rnd->shaders() );

    // Compile
    _rnd->shaders().compileAllShaders( threadCount );

    // Pipeline creation
    _mtlLib.createPipelines( _rnd->pipelines() );
    _passes.createPipelines( _rnd->pipelines() );

    auto endTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<float, std::milli> duration = endTime - startTime;

    AXION_LOG_INFO( Logger::Module::Core, "End of shader compilation for Renderer [{}]. Time elapsed: {:.2f} ms", _settings.common.name, duration.count() );
}

void Rasterizer::render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime ) {

    // Early Exit
    if ( !cameraEntity.hasComponent<Scene::CameraComponent>() || scene.getRegistry().view<Scene::MeshComponent>().empty() )
    {
        AXION_LOG_WARN_ONCE( Logger::Module::Core, "Scene [{}] given to Rasterizer [{}] is empty or Camera is missing.", scene.getName(), _settings.common.name );
        return;
    }

    GPUSceneUpdateFlags updateFlags = GPUSceneNone;
    if ( _settings.common.gfxApi == Graphics::API::DirectX12 )
        updateFlags |= GPUSceneTransposeMatrices;

    _gpuScene.update( scene,
                      cameraEntity,
                      _window->getSize(),
                      deltaTime,
                      updateFlags );

    auto currentUboBuffer = _rnd->resources().getBuffer( _res.uboBufferHandles[_rnd->getCurrentFrameIndex()] );
    auto transientOffsets = _gpuScene.uploadTransientData( currentUboBuffer );

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
        auto rtExtent = _window->getSettings().size.to3D();

        // A. Upload
        UploadPass::Config upConfig;
        upConfig.bufferHandles = {
            .vertex = builder.import( "GlobalVertexBuffer", _res.vertexBufferHandle ),
            .index  = builder.import( "GlobalIndexBuffer", _res.indexBufferHandle ) };
        upConfig.gpuScene          = &_gpuScene;
        upConfig.maxAllocationSize = _settings.memory.uploadBufferSize;

        _passes.getPass<UploadPass>()->addToGraph( builder, upConfig );

        // B. Forward
        ForwardPass::Config fwConfig;
        fwConfig.outputColorHandle = builder.texture( "ColorBuffer" )
                                         .asRenderTarget()
                                         .asStorage()
                                         .format( Graphics::Format::RGBA16_FLOAT )
                                         .extent( rtExtent )
                                         .clearValue( { .color = { 0.2f, 0.2f, 0.2f, 1.0f } } )
                                         .create();
        fwConfig.outputDepthHandle = builder.texture( "DepthBuffer" )
                                         .asDepthStencil()
                                         .format( Graphics::Format::D32 )
                                         .extent( rtExtent )
                                         .create();
        fwConfig.bufferHandles = {
            .vertex = upConfig.bufferHandles.vertex,
            .index  = upConfig.bufferHandles.index,
        };
        fwConfig.matLib          = &_mtlLib;
        fwConfig.matLayoutHandle = _globalMtlLayoutHandle;
        fwConfig.gpuScene        = &_gpuScene;
        fwConfig.uboOffsets      = transientOffsets;

        _passes.getPass<ForwardPass>()->addToGraph( builder, fwConfig );

        // C. ToneMapping
        ToneMappingPass::Config tmConfig;
        tmConfig.inputHandle  = fwConfig.outputColorHandle;
        tmConfig.outputHandle = builder.texture( "ToneMappedBuffer" )
                                    .format( _settings.common.backbufferFormat )
                                    .extent( rtExtent )
                                    .asStorage()
                                    .create();

        _passes.getPass<ToneMappingPass>()->addToGraph( builder, tmConfig );

        // D. Final Blit/Present
      
        cpypass.inputHandle  = tmConfig.outputHandle;
        cpypass.outputHandle = builder.import( "Backbuffer", _rnd->getCurrentBackbufferHandle() );
        builder.addPass( "FinalBlitPass", cpypass );
    } );
}

void Rasterizer::setupMaterialLibrary() {

    _globalMtlLayoutHandle = _rnd->pipelines().layout( "Global_Material_Layout" )
                                 // Space 0: Geometry
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 1 }, // Vertex
                                     { 1, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 1 }  // Index
                                 } )
                                 // Space 1: Scene Data
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::UniformBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Frame
                                     { 0, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 1 },  // Meshes
                                     { 1, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 1 },  // Instances
                                     { 2, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, 1 }   // Lights
                                 } )
                                 // Space 2: Materials
                                 //  .addSet( 2, { { DescriptorType::SRV, 0, 8 }, { DescriptorType::Sampler, 0, 2 } } )
                                 // Constants
                                 .setPushConstants( 128, 0, 2 )
                                 .create();

    _mtlLib.init( _settings.common.gfxApi );
    _mtlLib.setTargetLayout( _globalMtlLayoutHandle );
    _mtlLib.setPassFormats( MaterialPassType::Opaque,
                            MaterialPassProfile {
                                .renderTargetFormats = { Graphics::Format::RGBA16_FLOAT },
                                .depthTargetFormat   = _settings.depthFormat,
                            } );
}

void Rasterizer::registerMaterials() {

    // In the future we could reand the file p xml/python and define materials from it.
    // For now, we manually declare

    _mtlLib.beginMaterial( "TestMaterial" )
        .addPass( MaterialPassType::Opaque,
                  AXION_SHADER_DIR "/Slang/Materials/TestMaterial.slang",
                  { { "vsForward", Graphics::RHI::ShaderType::Vertex },
                    { "psForward", Graphics::RHI::ShaderType::Pixel } } )
        .finish();
}

void Rasterizer::registerPasses() {
    _passes.registerPass<UploadPass>();
    _passes.registerPass<ForwardPass>();
    _passes.registerPass<ToneMappingPass>();
}

void Rasterizer::createResources() {

    auto& r = _rnd->resources();

    // A. GLOBAL BUFFERS (Persistent)
    _res.vertexBufferHandle = r.buffer( "GlobalVertexBuffer" )
                                  .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                  .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                  .size( _settings.memory.geometryBufferSize )
                                  .onGPU()
                                  .asRaw()
                                  .create();
    _res.indexBufferHandle = r.buffer( "GlobalIndexBuffer" )
                                 .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                 .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                 .size( _settings.memory.geometryBufferSize )
                                 .onGPU()
                                 .asRaw()
                                 .create();
    _res.matBufferHandle = r.buffer( "GlobalMaterialBuffer" )
                               .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                               .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                               .size( _settings.memory.materialBufferSize )
                               .onGPU()
                               .asRaw()
                               .create();

    // NOTA: Recuerda cambiar esto a FreeListAllocator/BuddyAllocator en el futuro
    // _res.geomAllocator = Graphics::RHI::LinearAllocator( r.getBuffer( _res.geomBufferHandle ) );
    // _res.matAllocator  = Graphics::RHI::LinearAllocator( r.getBuffer( _res.matBufferHandle ) );

    // B. PER-FRAME BUFFERS (Volatile)
    _framesInFlight = _rnd->getTotalFramesInFlight();
    _res.uboBufferHandles.resize( _framesInFlight );
    _res.uboBufferAllocators.reserve( _framesInFlight );

    for ( uint i = 0; i < _framesInFlight; ++i )
    {
        _res.uboBufferHandles[i] = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                       .size( _settings.memory.volatileBufferSize )
                                       .onCPU()
                                       .create();

        _res.uboBufferAllocators.emplace_back( r.getBuffer( _res.uboBufferHandles[i] ) );
    }
}

} // namespace Core::Render

AXION_NAMESPACE_END