
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
        .debugMode             = ( settings.common.flags & RendererEnableDebug ) != RendererNone,
        .presentMode           = wnd->getSettings().flags & Platform::WindowVSync ? Graphics::PresentMode::Vsync : Graphics::PresentMode::Immediate,
        .backbufferFormat      = settings.common.backbufferFormat,
        .RGAllocSize           = settings.memory.RGAllocSize,
        .RGAllocSBTSize        = settings.memory.GPUCommandBuffersSize,
        .RGDescriptorsPerFrame = settings.memory.RGDescriptorsPerFrame,
        .RGMaxViewsPerFrame    = settings.memory.RGMaxViewsPerFrame,
        .RGMaxSamplersPerFrame = settings.memory.RGMaxSamplersPerFrame,
        .RGTransientAllocSize  = settings.memory.uploadBufferSize,
        .GCMode                = settings.common.GCMode,
        .autoSync              = true,
        .selectedDeviceID      = settings.common.selectedDeviceID,
        .enableGui             = ( settings.common.flags & RendererEnableGUI ) != RendererNone };

    const uint remainingVolatileViews = _settings.memory.RGMaxViewsPerFrame - settings.common.maxMtlTextures;
    AXION_LOG_ASSERT( remainingVolatileViews >= 256, Logger::Module::RHI, "Volatile Views Count is critically low!" );

    _rnd = Graphics::createRenderer( wnd->getNativeWindow(), rndStts );

    // Configure Material Library & Global Layout Contract
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

void Rasterizer::newGuiFrame() const {
    if ( _settings.common.flags & RendererEnableGUI )
        _rnd->getGUIBackend()->newFrame();
}

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

#pragma region Render

void Rasterizer::render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime ) {

    // Early Exit
    if ( !cameraEntity.hasComponent<Scene::CameraComponent>() || scene.getRegistry().view<Scene::MeshComponent>().empty() )
    {
        AXION_LOG_WARN_ONCE( Logger::Module::Core, "Scene [{}] given to Rasterizer [{}] is empty or Camera is missing.", scene.getName(), _settings.common.name );
        return;
    }

    GPUSceneUpdateFlags updateFlags = GPUSceneSortInstances;
    if ( _settings.common.gfxApi == Graphics::API::DirectX12 )
        updateFlags |= GPUSceneTransposeMatrices;

    _gpuScene.update( scene,
                      cameraEntity,
                      _mtlLib,
                      _window->getSize(),
                      deltaTime,
                      updateFlags );

    _res.textureHandles.resize( _gpuScene.textures().size() );

    auto& currentFrameRes = _res.frame[_rnd->getCurrentFrameIndex()];
    currentFrameRes.uboAllocator.reset();
    currentFrameRes.ssboAllocator.reset();
    currentFrameRes.indirectAllocator.reset();

    auto transientViews  = uploadTransientData( currentFrameRes.uboAllocator,
                                               currentFrameRes.ssboAllocator );
    auto indirectCmdData = uploadIndirectCommandData( currentFrameRes.ssboAllocator,
                                                      currentFrameRes.indirectAllocator );

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
        auto rtExtent = _window->getSettings().size.to3D();

        //----------------------------
        // A. Upload Global Data
        //----------------------------
        UploadPass::Config upConfig;
        upConfig.outGlobalBufferHandles = {
            .vertex    = builder.import( "GlobalVertexBuffer", _res.vertexBufferHandle ),
            .index     = builder.import( "GlobalIndexBuffer", _res.indexBufferHandle ),
            .materials = builder.import( "GlobalMaterialBuffer", _res.mtlBufferHandle ) };
        upConfig.gpuScene          = &_gpuScene;
        upConfig.vertexAllocator   = &_res.vertexAllocator;
        upConfig.indexAllocator    = &_res.indexAllocator;
        upConfig.matAllocator      = &_res.mtlAllocator;
        upConfig.maxAllocationSize = _settings.memory.uploadBufferSize;

        upConfig.mtlTextureHandles = &_res.textureHandles;

        for ( uint i = 0; i < _framesInFlight; ++i )
            upConfig.allPersistentSets.push_back( _res.frame[i].persistentDescriptorSetPtr );

        _passes.getPass<UploadPass>()->addToGraph( builder, upConfig );

        //----------------------------
        // B. GPU-Culling
        //----------------------------
        CullingPass::Config cullConfig;
        if ( _settings.useGPUCulling )
        {
            IndirectUploadPass::Config indUpConfig;
            indUpConfig.inOutIndirectBufferHandle         = builder.import( "IndirectCommandBuffer", currentFrameRes.indirectBufferHandle );
            indUpConfig.inOutIndirectTemplateBufferHandle = currentFrameRes.indirectTemplateBufferHandle;
            indUpConfig.indirectData                      = indirectCmdData;

            _passes.getPass<IndirectUploadPass>()->addToGraph( builder, indUpConfig );

            cullConfig.outIndirectBufferHandle       = indUpConfig.inOutIndirectBufferHandle;
            cullConfig.outCulledRedirectBufferHandle = builder.import( "IndirectCulledInstanceBuffer", currentFrameRes.culledInstanceBufferHandle );

            cullConfig.inFrameView       = transientViews.frameView;
            cullConfig.inMeshesView      = transientViews.meshesView;
            cullConfig.inInstancesView   = transientViews.instancesView;
            cullConfig.inRedirectionView = transientViews.redirectView;

            cullConfig.indirectData = indirectCmdData;

            cullConfig.instanceCount = (uint)_gpuScene.instances().size();

            _passes.getPass<CullingPass>()->addToGraph( builder, cullConfig );
        }

        //----------------------------
        // C. Depth Pre-Pass
        //----------------------------

        DepthPrePass::Config dpConfig;
        dpConfig.outDepthHandle = builder.texture( "DepthRT" )
                                      .asDepthStencil()
                                      .format( Graphics::Format::D32 )
                                      .extent( rtExtent )
                                      .create();
        dpConfig.inGlobalBufferHandles = {
            .vertex   = upConfig.outGlobalBufferHandles.vertex,
            .index    = upConfig.outGlobalBufferHandles.index,
            .material = upConfig.outGlobalBufferHandles.materials },
        dpConfig.matLib          = &_mtlLib;
        dpConfig.matLayoutHandle = _globalMtlLayoutHandle;

        dpConfig.inFrameView       = transientViews.frameView;
        dpConfig.inMeshesView      = transientViews.meshesView;
        dpConfig.inMaterialsView   = transientViews.mtlView;
        dpConfig.inInstancesView   = transientViews.instancesView;
        dpConfig.inLightsView      = transientViews.lightsView;
        dpConfig.inEnvsView        = transientViews.envsView;
        dpConfig.inRedirectionView = transientViews.redirectView;

        dpConfig.indirectData                 = indirectCmdData;
        dpConfig.inIndirectBufferHandle       = cullConfig.outIndirectBufferHandle;
        dpConfig.inCulledRedirectBufferHandle = cullConfig.outCulledRedirectBufferHandle;
        dpConfig.useGPUCulling                = _settings.useGPUCulling;

        dpConfig.persistentDescriptorSet = currentFrameRes.persistentDescriptorSetPtr;

        _passes.getPass<DepthPrePass>()->addToGraph( builder, dpConfig );

        //----------------------------
        // D. Forward
        //----------------------------
        ForwardPass::Config fwConfig;
        fwConfig.outColorHandle = builder.texture( "ColorRT" )
                                      .asRenderTarget()
                                      .asStorage()
                                      .format( Graphics::Format::RGBA16_FLOAT )
                                      .extent( rtExtent )
                                      .clearValue( { .color = { 0.2f, 0.2f, 0.2f, 1.0f } } )
                                      .create();
        fwConfig.outDepthHandle        = dpConfig.outDepthHandle;
        fwConfig.inGlobalBufferHandles = {
            .vertex   = dpConfig.inGlobalBufferHandles.vertex,
            .index    = dpConfig.inGlobalBufferHandles.index,
            .material = dpConfig.inGlobalBufferHandles.material },
        fwConfig.matLib          = &_mtlLib;
        fwConfig.matLayoutHandle = _globalMtlLayoutHandle;
        fwConfig.gpuScene        = &_gpuScene;

        fwConfig.inFrameView       = transientViews.frameView;
        fwConfig.inMeshesView      = transientViews.meshesView;
        fwConfig.inMaterialsView   = transientViews.mtlView;
        fwConfig.inInstancesView   = transientViews.instancesView;
        fwConfig.inLightsView      = transientViews.lightsView;
        fwConfig.inEnvsView        = transientViews.envsView;
        fwConfig.inRedirectionView = transientViews.redirectView;

        fwConfig.indirectData                 = indirectCmdData;
        fwConfig.inIndirectBufferHandle       = cullConfig.outIndirectBufferHandle;
        fwConfig.inCulledRedirectBufferHandle = cullConfig.outCulledRedirectBufferHandle;
        fwConfig.useGPUCulling                = _settings.useGPUCulling;

        fwConfig.persistentDescriptorSet = currentFrameRes.persistentDescriptorSetPtr;

        _passes.getPass<ForwardPass>()->addToGraph( builder, fwConfig );

        //----------------------------
        // E. ToneMapping
        //----------------------------
        ToneMappingPass::Config tmConfig;
        float                   ev100          = cameraEntity.getComponent<Scene::CameraComponent>().getEV100();
        float                   exposureFactor = 1.0f / ( 1.2f * std::pow( 2.0f, ev100 ) );
        tmConfig.exposure                      = exposureFactor;
        tmConfig.inputHandle                   = fwConfig.outColorHandle;
        tmConfig.outputHandle                  = builder.texture( "ToneMappedRT" )
                                    .format( _settings.common.backbufferFormat )
                                    .extent( rtExtent )
                                    .asStorage()
                                    .create();

        _passes.getPass<ToneMappingPass>()->addToGraph( builder, tmConfig );

        Graphics::RGResourceHandle currentBlitInput = tmConfig.outputHandle;

        //----------------------------
        // F. FXAA (Optional)
        //----------------------------

        if ( _settings.common.flags & RendererEnableFXAA )
        {
            FXAAPass::Config fxaaConfig;
            fxaaConfig.inputHandle  = tmConfig.outputHandle;
            fxaaConfig.outputHandle = builder.texture( "FxaaRT" )
                                          .format( _settings.common.backbufferFormat )
                                          .extent( rtExtent )
                                          .asStorage()
                                          .create();
            fxaaConfig.linearSamplerHandle = _res.fallbackSamplerHandle;

            _passes.getPass<FXAAPass>()->addToGraph( builder, fxaaConfig );
            currentBlitInput = fxaaConfig.outputHandle;
        }

        //----------------------------
        // F. Blit
        //----------------------------

        Graphics::RGResourceHandle backbufferHandle = builder.import( "BackbufferRT", _rnd->getCurrentBackbufferHandle() );

        _cpypass.inputHandle  = currentBlitInput;
        _cpypass.outputHandle = backbufferHandle;
        builder.addPass( "FinalBlitPass", _cpypass );

        //----------------------------
        // F. GUI Pass (Optional)
        //----------------------------
        if ( _settings.common.flags & RendererEnableGUI )
        {
            _guipass.guiBackend   = _rnd->getGUIBackend();
            _guipass.outputHandle = backbufferHandle;
            builder.addPass( "GUIPass", _guipass );
        }

        //----------------------------
        // F. Present Pass
        //----------------------------
        _presentpass.inoutHandle = backbufferHandle;
        builder.addPass( "PresentPass", _presentpass );
    } );
}

#pragma endregion
#pragma region Resources

void Rasterizer::setupMaterialLibrary() {

    _mtlLib.init( _settings.common.gfxApi, MaterialPassSupportDepth );

    // Global Layout (BINDLESS CONTRACT)
    _globalMtlLayoutHandle = _rnd->pipelines().layout( "Global_Material_Layout" )
                                 // Space 0: Persistent (Geometry, Materials and Textures)
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                      // Vertex
                                     { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                      // Index
                                     { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 },                      // Materials
                                     { 3, Graphics::RHI::DescriptorType::SampledImage, Graphics::RHI::ShaderStage::All, _settings.common.maxMtlTextures }, // Textures
                                     { 0, Graphics::RHI::DescriptorType::Sampler, Graphics::RHI::ShaderStage::All, _settings.common.maxMtlSamplers }       // Samplers
                                 } )
                                 // Space 1: Scene Data
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::UniformBuffer, Graphics::RHI::ShaderStage::All, 1 },         // Frame
                                     { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Meshes
                                     { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Materials
                                     { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Instances
                                     { 3, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Lights
                                     { 4, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Environments
                                     { 5, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }  // Instance Redirection Buffer
                                 } )
                                 // Space 2: Instance ID Push Constant
                                 .setPushConstants( sizeof( uint ), 0, 2 )
                                 .enableIndirectRendering()
                                 .create();

    _mtlLib.setTargetLayout( _globalMtlLayoutHandle );

    // Standard opaque pass
    _mtlLib.setPassFormats( MaterialPassType::Opaque,
                            MaterialPassProfile {
                                .renderTargetFormats = { Graphics::Format::RGBA16_FLOAT },
                                .depthTargetFormat   = _settings.depthFormat,
                            } );
    // Depth pre-pass
    _mtlLib.setPassFormats( MaterialPassType::Depth,
                            MaterialPassProfile {
                                .depthTargetFormat = _settings.depthFormat,
                            } );
}

void Rasterizer::registerMaterials() {

    Assets::GlobalMaterialRegistry::enumerate(
        [&]( const std::string& name, Assets::GlobalMaterialRegistry::SetupCallback setupFunc ) {
            MaterialArchetypeDesc desc;

            setupFunc( desc );

            _mtlLib.registerArchetype( desc );
        } );

    // _mtlLib.beginMaterial( "ErrorMaterial" )
    //     .addPass( MaterialPassType::Opaque,
    //               AXION_SHADER_DIR "/Slang/Materials/Test.slang",
    //               { { "vsForward", Graphics::ShaderType::Vertex },
    //                 { "psForward", Graphics::ShaderType::Pixel } } )
    //     .finish();
}

void Rasterizer::registerPasses() {
    _passes.registerPass<UploadPass>();
    _passes.registerPass<IndirectUploadPass>();
    _passes.registerPass<CullingPass>();
    _passes.registerPass<DepthPrePass>();
    _passes.registerPass<ForwardPass>();
    _passes.registerPass<ToneMappingPass>();
    _passes.registerPass<FXAAPass>();
}

void Rasterizer::createResources() {

    auto& r = _rnd->resources();

    //------------------------- A. GLOBAL BUFFERS (Persistent) -------------------
    _res.vertexBufferHandle = r.buffer( "GlobalVertexBuffer" )
                                  .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                  .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                  .size( _settings.memory.geometryBufferSize )
                                  .onGPU()
                                  .asRaw()
                                  .create();
    _res.vertexAllocator = Graphics::RHI::FreeListAllocator( r.getBuffer( _res.vertexBufferHandle ) );

    _res.indexBufferHandle = r.buffer( "GlobalIndexBuffer" )
                                 .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                 .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                 .size( _settings.memory.geometryBufferSize )
                                 .onGPU()
                                 .asRaw()
                                 .asIBO()
                                 .create();
    _res.indexAllocator = Graphics::RHI::FreeListAllocator( r.getBuffer( _res.indexBufferHandle ) );

    _res.mtlBufferHandle = r.buffer( "GlobalMaterialBuffer" )
                               .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                               .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                               .size( _settings.memory.materialBufferSize )
                               .onGPU()
                               .asRaw()
                               .create();
    _res.mtlAllocator = Graphics::RHI::FreeListAllocator( r.getBuffer( _res.mtlBufferHandle ) );

    //------------------------- B. Fallback Resources -------------------

    std::array<uchar, 4> fallbackPixels = { 255, 0, 255, 255 };
    _res.fallbackTexture2DHandle        = r.texture( "FallbackTexture2D" )
                                       .format( Graphics::Format::RGBA8_UNORM )
                                       .extent( { 1, 1, 1 } )
                                       .withData( fallbackPixels.data() )
                                       .create();

    _res.fallbackSamplerHandle = r.sampler( "FallbackSampler" ).create();

    std::vector<Graphics::RHI::ITexture*> initialTextures( _settings.common.maxMtlTextures, r.getTexture( _res.fallbackTexture2DHandle ) );
    std::vector<Graphics::RHI::ISampler*> initialSamplers( _settings.common.maxMtlSamplers, r.getSampler( _res.fallbackSamplerHandle ) );

    // ----------------------- C. PER-FRAME BUFFERS (Volatile) -------------------
    _framesInFlight = _rnd->getTotalFramesInFlight();
    _res.frame.resize( _framesInFlight );
    for ( uint i = 0; i < _framesInFlight; ++i )
    {
        _res.frame[i].uboBufferHandle = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                            .size( 1024 )
                                            .onCPU()
                                            .create();
        auto* uboBuffer = r.getBuffer( _res.frame[i].uboBufferHandle );
        uboBuffer->map();
        _res.frame[i].uboAllocator = Graphics::RHI::LinearAllocator( uboBuffer );

        _res.frame[i].ssboBufferHandle = r.buffer( "GlobalSSBO_" + std::to_string( i ) )
                                             .size( _settings.memory.volatileBufferSize )
                                             .onCPU()
                                             .create();

        auto* ssboBuffer = r.getBuffer( _res.frame[i].ssboBufferHandle );
        ssboBuffer->map();
        _res.frame[i].ssboAllocator = Graphics::RHI::LinearAllocator( ssboBuffer );

        // ----------------------- D. PER-FRAME INDIRECT RENDERING BUFFERS (Volatile) -------------------
        _res.frame[i].indirectStagingBufferHandle = r.buffer( "IndirectStagingCommandBuffer_" + std::to_string( i ) )
                                                        .size( _settings.memory.GPUCommandBuffersSize )
                                                        .onCPU()
                                                        .create();
        auto* indBuffer = r.getBuffer( _res.frame[i].indirectStagingBufferHandle );
        indBuffer->map();
        _res.frame[i].indirectAllocator = Graphics::RHI::LinearAllocator( indBuffer );

        _res.frame[i].indirectBufferHandle = r.buffer( "IndirectCommandBuffer_" + std::to_string( i ) )
                                                 .size( _settings.memory.GPUCommandBuffersSize )
                                                 .asSSBO()
                                                 .stride( sizeof( Graphics::RHI::DrawIndexedIndirectCommand ) )
                                                 .create();
        _res.frame[i].indirectTemplateBufferHandle = r.buffer( "IndirectTemplateCommandBuffer_" + std::to_string( i ) )
                                                         .size( _settings.memory.GPUCommandBuffersSize )
                                                         .asSSBO()
                                                         .stride( sizeof( Graphics::RHI::DrawIndexedIndirectCommand ) )
                                                         .create();

        _res.frame[i].culledInstanceBufferHandle = r.buffer( "IndirectCulledInstanceBuffer_" + std::to_string( i ) )
                                                       .size( _settings.memory.volatileBufferSize )
                                                       .stride( sizeof( uint ) )
                                                       .onGPU()
                                                       .asSSBO()
                                                       .create();

        // ----------------------- E. PER-FRAME DESCRIPTOR SET (Persistent) -------------------
        auto* frameDescriptorAllocator = _rnd->getFrameDescriptorAllocator( i );

        auto* persistentSet = frameDescriptorAllocator->allocate( _rnd->pipelines().getLayout( _globalMtlLayoutHandle ), 0 );

        // Attach core persistent buffers
        persistentSet->attach( 0, r.getBuffer( _res.vertexBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        persistentSet->attach( 1, r.getBuffer( _res.indexBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        persistentSet->attach( 2, r.getBuffer( _res.mtlBufferHandle ), Graphics::RHI::ResourceState::ShaderResource );
        persistentSet->attachBindlessArray( 3, 0, initialTextures, Graphics::RHI::ResourceState::ShaderResource );
        persistentSet->attachBindlessArray( 0, 0, initialSamplers );

        frameDescriptorAllocator->lockPersistent();

        // Store Persistent Descriptor Set Ptr
        _res.frame[i].persistentDescriptorSetPtr = persistentSet;
    }
}

#pragma endregion
#pragma region CPU-GPU Uploads

Rasterizer::TransientViews Rasterizer::uploadTransientData( Graphics::RHI::LinearAllocator& currentUBOAlloc,
                                                            Graphics::RHI::LinearAllocator& currentSSBOAlloc ) {
    TransientViews views;

    // (D3D12/Vulkan)
    const uint CBV_ALIGNMENT = 256;

    // =================================================================================
    // 1. FRAME DATA (UBO - Constant Buffer)
    // =================================================================================
    {
        const auto& frameData = _gpuScene.frame();

        views.frameView = currentUBOAlloc.allocate( sizeof( GPUFrame ), CBV_ALIGNMENT );

        if ( views.frameView.isValid() )
        {
            memcpy( views.frameView.cpuAddress, &frameData, sizeof( GPUFrame ) );
            views.frameView.stride = sizeof( GPUFrame );
            views.frameView.count  = 1;
        }
    }

    // =================================================================================
    // 2. MESH DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& meshes = _gpuScene.meshes();

        if ( !meshes.empty() )
        {
            views.meshesView = currentSSBOAlloc.allocate<GPUMesh>( meshes.size() );

            if ( views.meshesView.isValid() )
            {
                memcpy( views.meshesView.cpuAddress, meshes.data(), views.meshesView.size );
            }
        } else
        {
            // Dummy
            views.meshesView       = currentSSBOAlloc.allocate<GPUMesh>( 1 );
            views.meshesView.count = 0; // Empty
            if ( views.meshesView.cpuAddress )
                memset( views.meshesView.cpuAddress, 0, views.meshesView.size );
        }
    }

    // =================================================================================
    // 3. MATERIAL DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& mtls = _gpuScene.materials();

        if ( !mtls.empty() )
        {
            views.mtlView = currentSSBOAlloc.allocate<GPUMaterial>( mtls.size() );

            if ( views.mtlView.isValid() )
            {
                memcpy( views.mtlView.cpuAddress, mtls.data(), views.mtlView.size );
            }
        } else
        {
            // DUMMY
            views.mtlView       = currentSSBOAlloc.allocate<GPUMaterial>( 1 );
            views.mtlView.count = 0;
            if ( views.mtlView.cpuAddress )
                memset( views.mtlView.cpuAddress, 0, views.mtlView.size );
        }
    }
    // =================================================================================
    // 4. INSTANCE DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& instances = _gpuScene.instances();

        if ( !instances.empty() )
        {
            views.instancesView = currentSSBOAlloc.allocate<GPUInstance>( instances.size() );

            if ( views.instancesView.isValid() )
            {
                memcpy( views.instancesView.cpuAddress, instances.data(), views.instancesView.size );
            }
        } else
        {
            // DUMMY
            views.instancesView       = currentSSBOAlloc.allocate<GPUInstance>( 1 );
            views.instancesView.count = 0;
            if ( views.instancesView.cpuAddress )
                memset( views.instancesView.cpuAddress, 0, views.instancesView.size );
        }
    }

    // =================================================================================
    // 5. LIGHT DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& lights = _gpuScene.lights();

        if ( !lights.empty() )
        {
            views.lightsView = currentSSBOAlloc.allocate<GPULight>( lights.size() );

            if ( views.lightsView.isValid() )
            {
                memcpy( views.lightsView.cpuAddress, lights.data(), views.lightsView.size );
            }
        } else
        {
            // DUMMY
            views.lightsView       = currentSSBOAlloc.allocate<GPULight>( 1 );
            views.lightsView.count = 0;
            if ( views.lightsView.cpuAddress )
                memset( views.lightsView.cpuAddress, 0, views.lightsView.size );
        }
    }
    // =================================================================================
    // 6. ENV DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& envs = _gpuScene.environments();

        if ( !envs.empty() )
        {
            views.envsView = currentSSBOAlloc.allocate<GPUEnvironment>( envs.size() );

            if ( views.envsView.isValid() )
            {
                memcpy( views.envsView.cpuAddress, envs.data(), views.envsView.size );
            }
        } else
        {
            // DUMMY
            views.envsView       = currentSSBOAlloc.allocate<GPUEnvironment>( 1 );
            views.envsView.count = 0;
            if ( views.envsView.cpuAddress )
                memset( views.envsView.cpuAddress, 0, views.envsView.size );
        }
    }

    // =================================================================================
    // 7. INSTANCE REDIRECTION DATA (for true instancing) (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& sortedKeys = _gpuScene.getSortedKeys();

        if ( !sortedKeys.empty() )
        {
            views.redirectView = currentSSBOAlloc.allocate<uint>( sortedKeys.size() );

            if ( views.redirectView.isValid() )
            {
                auto* redirectPtr = (uint*)views.redirectView.cpuAddress;

                for ( size_t i = 0; i < sortedKeys.size(); ++i )
                {
                    redirectPtr[i] = sortedKeys[i].originalInstanceIdx;
                }
            }
        } else
        {
            views.redirectView       = currentSSBOAlloc.allocate<uint>( 1 );
            views.redirectView.count = 0;
            if ( views.redirectView.cpuAddress )
                memset( views.redirectView.cpuAddress, 0, views.redirectView.size );
        }
    }

    return views;
}

IndirectCommandData Rasterizer::uploadIndirectCommandData(
    Graphics::RHI::LinearAllocator& currentSSBOAlloc,
    Graphics::RHI::LinearAllocator& currentIndirectAlloc ) {
    const auto& sortedKeys = _gpuScene.getSortedKeys();
    const auto& instances  = _gpuScene.instances();
    const auto& meshes     = _gpuScene.meshes();

    IndirectCommandData indirectData;
    if ( sortedKeys.empty() )
        return indirectData;

    // 1. ALLOCATIONS
    // =================================================================================

    size_t maxPossibleCommands = meshes.size() * _mtlLib.getArchetypesCount();
    maxPossibleCommands        = std::max<size_t>( maxPossibleCommands, 1 );

    auto cmdAlloc = currentIndirectAlloc.allocate<Graphics::RHI::DrawIndexedIndirectCommand>( maxPossibleCommands );

    if ( !cmdAlloc.isValid() )
        return indirectData;

    auto*      cmdPtr       = (Graphics::RHI::DrawIndexedIndirectCommand*)cmdAlloc.cpuAddress;
    const auto RHI_CMD_SIZE = sizeof( Graphics::RHI::DrawIndexedIndirectCommand );

    indirectData.batches.reserve( _mtlLib.getArchetypesCount() ); // Optimistic

    // 2. LOOP
    // =================================================================================

    uint currentArch = 0, currentTopo = 0, currentMeshID = 0;
    sortedKeys[0].unpack( currentArch, currentTopo, currentMeshID );

    auto lastMesh = meshes[instances[sortedKeys[0].originalInstanceIdx].meshID];

    // Trackers
    uint batchStartOffsetInRedirect = 0;
    uint instanceAccumulator        = 0;

    uint cmdWriteIdx          = 0;
    uint cmdsInCurrentArch    = 0;
    uint archBatchStartCmdIdx = 0;

    uint*                     indirectCmdMapPtr = nullptr;
    Graphics::RHI::BufferView cmdMapAlloc {};
    if ( _settings.useGPUCulling )
    {
        cmdMapAlloc       = currentSSBOAlloc.allocate<uint>( instances.size() );
        indirectCmdMapPtr = (uint*)cmdMapAlloc.cpuAddress;
    }

    for ( size_t i = 0; i < sortedKeys.size(); ++i )
    {
        uint arch, topo, meshID;
        sortedKeys[i].unpack( arch, topo, meshID );

        bool breakInstancing = ( meshID != currentMeshID ) ||
                               ( arch != currentArch ) ||
                               ( topo != currentTopo );

        if ( breakInstancing && instanceAccumulator > 0 )
        {
            Graphics::RHI::DrawIndexedIndirectCommand cmd;
            cmd.indexCount    = lastMesh.indexCount;
            cmd.instanceCount = _settings.useGPUCulling ? 0 : instanceAccumulator;
            cmd.firstIndex    = lastMesh.indexOffset / 4;
            cmd.vertexOffset  = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
            cmd.firstInstance = 0;

            cmd.baseInstanceID = batchStartOffsetInRedirect;

            cmdPtr[cmdWriteIdx] = cmd;

            cmdWriteIdx++;
            cmdsInCurrentArch++;

            // Reset
            instanceAccumulator        = 0;
            batchStartOffsetInRedirect = (uint)i;

            // Update Trackers
            currentMeshID    = meshID;
            uint originalIdx = sortedKeys[i].originalInstanceIdx;
            lastMesh         = meshes[instances[originalIdx].meshID];
        }

        if ( arch != currentArch || topo != currentTopo )
        {
            indirectData.batches.push_back( { .archetypeID  = currentArch,
                                              .topologyID   = currentTopo,
                                              .bufferOffset = (uint)( cmdAlloc.offset + ( archBatchStartCmdIdx * RHI_CMD_SIZE ) ),
                                              .drawCount    = cmdsInCurrentArch } );

            currentArch          = arch;
            currentTopo          = topo;
            cmdsInCurrentArch    = 0;
            archBatchStartCmdIdx = cmdWriteIdx;
        }

        if ( _settings.useGPUCulling )
            indirectCmdMapPtr[i] = cmdWriteIdx;

        instanceAccumulator++;
    }

    // 3. Close
    // =================================================================================

    if ( instanceAccumulator > 0 )
    {
        Graphics::RHI::DrawIndexedIndirectCommand cmd;
        cmd.indexCount    = lastMesh.indexCount;
        cmd.instanceCount = _settings.useGPUCulling ? 0 : instanceAccumulator;
        cmd.firstIndex    = lastMesh.indexOffset / 4;
        cmd.vertexOffset  = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
        cmd.firstInstance = 0;

        cmd.baseInstanceID = batchStartOffsetInRedirect;

        cmdPtr[cmdWriteIdx++] = cmd;
        cmdsInCurrentArch++;
    }

    // Último Render Batch
    if ( cmdsInCurrentArch > 0 )
    {
        indirectData.batches.push_back( { .archetypeID  = currentArch,
                                          .topologyID   = currentTopo,
                                          .bufferOffset = (uint)( cmdAlloc.offset + ( archBatchStartCmdIdx * RHI_CMD_SIZE ) ),
                                          .drawCount    = cmdsInCurrentArch } );
    }

    // 4. Finish
    // =================================================================================

    indirectData.commandBufferView      = cmdAlloc;
    indirectData.commandBufferView.size = cmdWriteIdx * RHI_CMD_SIZE;
    indirectData.batchMapView           = cmdMapAlloc;

    if ( !_settings.useGPUCulling )
        return indirectData;

#ifdef AXION_DEBUG
    indirectData.dirty = true;

#else
    size_t currentCmdCount = cmdWriteIdx;
    size_t currentMapCount = instances.size();

    bool structuralChanges = false;

    if ( currentCmdCount != _indirectCommandDataCache.commands.size() )
        structuralChanges = true;
    else if ( std::memcmp( cmdPtr, _indirectCommandDataCache.commands.data(), currentCmdCount * sizeof( Graphics::RHI::DrawIndexedIndirectCommand ) ) != 0 )
        structuralChanges = true;

    if ( !structuralChanges )
    {
        if ( currentMapCount != _indirectCommandDataCache.batchMap.size() )
            structuralChanges = true;
        else if ( std::memcmp( indirectCmdMapPtr, _indirectCommandDataCache.batchMap.data(), currentMapCount * sizeof( uint ) ) != 0 )
            structuralChanges = true;
    }

    if ( structuralChanges )
    {
        _indirectCommandDataCache.commands.assign( cmdPtr, cmdPtr + currentCmdCount );

        _indirectCommandDataCache.batchMap.assign( indirectCmdMapPtr, indirectCmdMapPtr + currentMapCount );

        //  CPU->GPU
        indirectData.dirty = true;
    } else
    {
        // CPU->GPU
        indirectData.dirty = false;
    }

#endif

    return indirectData;
}
} // namespace Core::Render

AXION_NAMESPACE_END