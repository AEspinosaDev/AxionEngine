
#include <Render/Rasterizer/Rasterizer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {
// Factory
RendererOwnerPtr createRasterizer( Platform::Window* wnd, const RasterizerSettings& settings ) {
    return Memory::makeOwned<Rasterizer::Rasterizer>( wnd, settings );
}
} // namespace Core::Render
namespace Core::Render::Rasterizer {

Rasterizer::Rasterizer( Platform::Window* wnd, const RasterizerSettings& settings )
    : _window( wnd )
    , _settings( settings )
    , _FRAMES_IN_FLIGHT( static_cast<u32>( _settings.common.bufferingType ) + 1 ) {

    AXION_LOG_ASSERT( wnd, Logger::Module::Core, "Window is null" );

    auto lowLevelMemoryBudget = convertMemoryBudget();

#ifdef AXION_DEBUG
    {
        const u64 totalVRAM = lowLevelMemoryBudget.device.maxTextureAlloc +
                              lowLevelMemoryBudget.device.maxBufferAlloc +
                              lowLevelMemoryBudget.device.maxRenderTargetAlloc;

        const u64 totalUpload = lowLevelMemoryBudget.device.maxUploadAlloc;

        const u64 totalHostRAM = _settings.memory.host.maxPersistentAlloc +
                                 ( _settings.memory.host.maxTransientAllocPerFrame * _FRAMES_IN_FLIGHT );

        AXION_LOG_INFO( Logger::Module::Core, "--- Rasterizer Memory Budget Initialization ---" );
        AXION_LOG_INFO( Logger::Module::Core, "Total Dedicated VRAM (Textures, Buffers, RTs): {} MB", totalVRAM / ( 1024 * 1024 ) );
        AXION_LOG_INFO( Logger::Module::Core, "Total Mapped Upload Memory (PCIe): {} MB", totalUpload / ( 1024 * 1024 ) );
        AXION_LOG_INFO( Logger::Module::Core, "Total Host RAM (Persistent + Transientx{}): {} MB", _FRAMES_IN_FLIGHT, totalHostRAM / ( 1024 * 1024 ) );
        AXION_LOG_INFO( Logger::Module::Core, "----------------------------------------" );
    }
#endif

    // Create Low-Level Renderer
    Graphics::RendererSettings rndStts = {
        .gfxApi                   = settings.common.gfxApi,
        .bufferingType            = settings.common.bufferingType,
        .debugMode                = ( settings.common.flags & RendererEnableDebug ) != RendererNone,
        .presentMode              = wnd->getSettings().flags & Platform::WindowVSync ? Graphics::PresentMode::Vsync : Graphics::PresentMode::Immediate,
        .backbufferFormat         = settings.common.backbufferFormat,
        .memory                   = lowLevelMemoryBudget,
        .RGmaxAlloc               = KBYTES( 1024 ),
        .RGmaxSBTAlloc            = settings.memory.shared.maxExecutableAlloc,
        .RGmaxTransientAlloc      = settings.memory.shared.maxUploadAllocPerFrame,
        .RGmaxDescriptorsPerFrame = 2048,
        .RGmaxViewsPerFrame       = Config::MAX_SHADER_RESOURCE_VIEWS,
        .RGmaxSamplersPerFrame    = Config::MAX_SAMPLER_VIEWS,
        .GCMode                   = settings.common.GCMode,
        .autoSync                 = true,
        .selectedDeviceID         = settings.common.selectedDeviceID,
        .enableGui                = ( settings.common.flags & RendererEnableGUI ) != RendererNone };

    _rnd = Graphics::createRenderer( wnd->getNativeWindow(), rndStts );

    // Configure Material Library & Global Layout Contract
    _globalMtlLayoutHandle = Config::buildGlobalLayout( _rnd->pipelines() );

    MaterialLibraryDesc matLibDesc;
    Config::matLibConfig( _globalMtlLayoutHandle, _settings, matLibDesc );
    _mtlLib.initialize( matLibDesc );

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

u64 Rasterizer::getCurrentFrameIndex() const {
    return _rnd->getCurrentFrameIndex();
}

u64 Rasterizer::getTotalFrameNumber() const {
    return _rnd->getTotalFrameNumber();
}

STLW::String Rasterizer::toString() const {
    return STLW::String();
}

void Rasterizer::compileShaders( u32 threadCount ) {

    auto startTime = std::chrono::high_resolution_clock::now();
    AXION_LOG_INFO( Logger::Module::Core, "Start of shader compilation for Renderer [{}] | Num Threads: {}", _settings.common.name, threadCount );

    // Register Material & Pass Shaders
    _mtlLib.registerShaders( _rnd->shaders() );
    _passes.registerShaders( _rnd->shaders() );

    // Compile
    _rnd->shaders().compileAllShaders( threadCount );

    // Pipeline creation for passes
    _passes.createPipelines( _rnd->pipelines() );

    auto endTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<float, std::milli> duration = endTime - startTime;

    AXION_LOG_INFO( Logger::Module::Core, "End of shader compilation for Renderer [{}]. Time elapsed: {:.2f} ms", _settings.common.name, duration.count() );
}

#pragma region Render

void Rasterizer::render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime ) {

    // Early Exit Logic
    if ( !cameraEntity.hasComponent<Scene::CameraComponent>() || scene.getRegistry().view<Scene::MeshComponent>().empty() )
    {
        AXION_LOG_WARN_ONCE( Logger::Module::Core, "Scene [{}] given to Rasterizer [{}] is empty or Camera is missing.", scene.getName(), _settings.common.name );

        _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
            Graphics::RGResourceHandle backbufferHandle = builder.import( "BackbufferRT", _rnd->getCurrentBackbufferHandle() );

            struct PassData {
                Graphics::RGResourceHandle target;
            };

            builder.addPass<PassData>( "ClearPass", [&]( Graphics::RenderPassBuilder& pb, PassData& data ) { data.target = pb.write( backbufferHandle, Graphics::RHI::ResourceState::RenderTarget ); },

                                       [&]( const PassData& data, Graphics::RenderPassContext& ctx ) {
                    
                auto* tex = ctx.getTexture( data.target );
                
                ctx.cmd->barrier( tex, Graphics::RHI::ResourceState::RenderTarget );
                ctx.cmd->clearTexture( tex, Graphics::ClearValue { .color = { 0.0f, 0.0f, 0.0f, 0.0f } } ); } );

            if ( _settings.common.flags & RendererEnableGUI )
            {
                _guipass.guiBackend   = _rnd->getGUIBackend();
                _guipass.outputHandle = backbufferHandle;
                builder.addPass( "GUIPass", _guipass );
            }

            _presentpass.inoutHandle = backbufferHandle;
            builder.addPass( "PresentPass", _presentpass );
        } );
        return;
    }

    // Scene Update
    GPUSceneUpdateFlags updateFlags = GPUSceneSortInstances;
    if ( _settings.common.gfxApi == Graphics::API::DirectX12 )
        updateFlags |= GPUSceneTransposeMatrices;

    _gpuScene.update( scene,
                      cameraEntity,
                      _mtlLib,
                      _window->getSize(),
                      deltaTime,
                      updateFlags );

    _res.texture2DHandles.resize( _gpuScene.textures().size() );
    _res.texture3DHandles.resize( Config::MAX_PERSISTENT_3D_TEXTURES );
    _res.textureCubeHandles.resize( Config::MAX_PERSISTENT_CUBE_TEXTURES );

    // Material Library Update
    _mtlLib.updatePipelines( _rnd->pipelines() );

    // Reset allocators
    const u32 FRAME_ID        = _rnd->getCurrentFrameIndex();
    auto&     currentFrameRes = _res.frame[FRAME_ID];
    currentFrameRes.uboAllocator.reset();
    currentFrameRes.ssboAllocator.reset();
    currentFrameRes.indirectAllocator.reset();

    // Upload CPU Coherent Data to GPU
    auto transientPayload   = uploadTransientData( currentFrameRes.uboAllocator,
                                                 currentFrameRes.ssboAllocator );
    auto indirectCmdPayload = uploadIndirectCommandData( currentFrameRes.ssboAllocator,
                                                         currentFrameRes.indirectAllocator );

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
        const Extent3D SCENE_RESOLUTION  = _window->getSettings().size.to3D();
        const Extent3D SCREEN_RESOLUTION = _window->getSettings().size.to3D();
        // const bool     GPU_CULLING_ENABLED = _settings.common.flags & RendererEnableGPUCulling;
        const bool GPU_CULLING_ENABLED = false;

        //----------------------------
        // 0. Allocate Frame Descriptor Set
        //----------------------------
        Graphics::RHI::IDescriptorAllocator* frameDescriptorAllocator = _rnd->getFrameDescriptorAllocator( FRAME_ID );
        Graphics::RHI::IDescriptorSet*       transientSetPtr          = frameDescriptorAllocator->allocate( _rnd->pipelines().getLayout( _globalMtlLayoutHandle ),
                                                                                             (u32)Config::DescriptorSetFrequency::FrameTransient );

        transientSetPtr->attachBufferSlice( 0, Graphics::RHI::DescriptorType::CBV, transientPayload.frameSlice );
        transientSetPtr->attachBufferSlice( 1, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.meshesSlice );
        transientSetPtr->attachBufferSlice( 2, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.mtlSlice );
        transientSetPtr->attachBufferSlice( 3, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.instancesSlice );
        transientSetPtr->attachBufferSlice( 4, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.lightsSlice );
        transientSetPtr->attachBufferSlice( 5, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.envsSlice );

        if ( GPU_CULLING_ENABLED )
        {

            // Graphics::BufferSlice culledSlice;
            // culledSlice.container = culledBuf;
            // culledSlice.offset    = 0;
            // culledSlice.size      = data.inRedirectionSlice.size;
            // culledSlice.stride    = data.inRedirectionSlice.stride;
            // transientSetPtr->attachBufferSlice( 6, culledSlice, Graphics::RHI::ResourceState::ShaderResource );
        } else
            transientSetPtr->attachBufferSlice( 6, Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.redirectSlice );

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
        upConfig.maxAllocationSize = _settings.memory.shared.maxUploadAllocPerFrame;

        upConfig.mtlTexture2DHandles = &_res.texture2DHandles;
        // TBD: Do the same for 3D and Cube textures

        for ( u32 i = 0; i < _FRAMES_IN_FLIGHT; ++i )
            upConfig.allPersistentSets.pushBack( _res.frame[i].persistentDescriptorSetPtr );

        _passes.getPass<UploadPass>()->addToGraph( builder, upConfig );

        //----------------------------
        // B. GPU-Culling
        //----------------------------
        CullingPass::Config cullConfig;
        if ( GPU_CULLING_ENABLED )
        {
            IndirectUploadPass::Config indUpConfig;
            indUpConfig.inOutIndirectBufferHandle         = builder.import( "IndirectCommandBuffer", currentFrameRes.indirectBufferHandle );
            indUpConfig.inOutIndirectTemplateBufferHandle = currentFrameRes.indirectTemplateBufferHandle;
            indUpConfig.indirectData                      = indirectCmdPayload;

            _passes.getPass<IndirectUploadPass>()->addToGraph( builder, indUpConfig );

            cullConfig.outIndirectBufferHandle       = indUpConfig.inOutIndirectBufferHandle;
            cullConfig.outCulledRedirectBufferHandle = builder.import( "IndirectCulledInstanceBuffer", currentFrameRes.culledInstanceBufferHandle );

            cullConfig.inFrameSlice       = transientPayload.frameSlice;
            cullConfig.inMeshesSlice      = transientPayload.meshesSlice;
            cullConfig.inInstancesSlice   = transientPayload.instancesSlice;
            cullConfig.inRedirectionSlice = transientPayload.redirectSlice;

            cullConfig.indirectData = indirectCmdPayload;

            cullConfig.instanceCount = (u32)_gpuScene.instances().size();

            _passes.getPass<CullingPass>()->addToGraph( builder, cullConfig );
        }

        //----------------------------
        // C. Vis Pre-Pass
        //----------------------------

        VisPass::Config visConfig;
        visConfig.outVisHandle = builder.texture( "VisRTO" )
                                     .asRenderTarget()
                                     .format( Graphics::Format::RG32_UINT )
                                     .extent( SCENE_RESOLUTION )
                                     .create();
        visConfig.outVelocityHandle = builder.texture( "VelocityRTO" )
                                          .asRenderTarget()
                                          .format( Graphics::Format::RG16_FLOAT )
                                          .extent( SCENE_RESOLUTION )
                                          .create();
        visConfig.outDepthHandle = builder.texture( "DepthRTO" )
                                       .asDepthStencil()
                                       .format( Graphics::Format::D32 )
                                       .extent( SCENE_RESOLUTION )
                                       .create();

        visConfig.inGlobalBufferHandles = {
            .vertex = upConfig.outGlobalBufferHandles.vertex,
            .index  = upConfig.outGlobalBufferHandles.index,
        },
        visConfig.matLib           = &_mtlLib;
        visConfig.materialPassSlot = (u32)Config::MaterialPassType::Visibility;

        visConfig.indirectData                 = indirectCmdPayload;
        visConfig.inIndirectBufferHandle       = cullConfig.outIndirectBufferHandle;
        visConfig.inCulledRedirectBufferHandle = cullConfig.outCulledRedirectBufferHandle;
        visConfig.useGPUCulling                = GPU_CULLING_ENABLED;

        visConfig.persistentDescriptorSet = currentFrameRes.persistentDescriptorSetPtr;
        visConfig.transientDescriptorSet  = transientSetPtr;

        _passes.getPass<VisPass>()->addToGraph( builder, visConfig );

        //----------------------------
        // D. Resolve Vis
        //----------------------------
        VisResolvePass::Config resConfig;
        resConfig.outColorHandle = builder.texture( "ColorRTO" )
                                       .asRenderTarget()
                                       .asStorage()
                                       .format( Graphics::Format::RGBA16_FLOAT )
                                       .extent( SCENE_RESOLUTION )
                                       .clearValue( { .color = { 0.2f, 0.2f, 0.2f, 1.0f } } )
                                       .create();
        resConfig.inVisHandle = visConfig.outVisHandle;

        resConfig.ioSetId = (u32)Config::DescriptorSetFrequency::PassTransient;

        resConfig.materialPassSlot = (u32)Config::MaterialPassType::VisibilityResolve;
        resConfig.matLib           = &_mtlLib;

        resConfig.persistentDescriptorSet = currentFrameRes.persistentDescriptorSetPtr;
        resConfig.transientDescriptorSet  = transientSetPtr;

        _passes.getPass<VisResolvePass>()->addToGraph( builder, resConfig );

        //----------------------------
        // E. ToneMapping
        //----------------------------
        ToneMappingPass::Config tmConfig;
        float                   ev100          = cameraEntity.getComponent<Scene::CameraComponent>().getEV100();
        float                   exposureFactor = 1.0f / ( 1.2f * std::pow( 2.0f, ev100 ) );
        tmConfig.exposure                      = exposureFactor;
        tmConfig.inputHandle                   = resConfig.outColorHandle;
        tmConfig.outputHandle                  = builder.texture( "ToneMappedRTO" )
                                    .format( _settings.common.backbufferFormat )
                                    .extent( SCENE_RESOLUTION )
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
            fxaaConfig.outputHandle = builder.texture( "FxaaRTO" )
                                          .format( _settings.common.backbufferFormat )
                                          .extent( SCENE_RESOLUTION )
                                          .asStorage()
                                          .create();
            fxaaConfig.linearSamplerHandle = _res.fallbackSamplerHandle;

            _passes.getPass<FXAAPass>()->addToGraph( builder, fxaaConfig );
            currentBlitInput = fxaaConfig.outputHandle;
        }

        //----------------------------
        // F. Blit
        //----------------------------

        Graphics::RGResourceHandle backbufferHandle = builder.import( "BackbufferRTO", _rnd->getCurrentBackbufferHandle() );

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

void Rasterizer::registerMaterials() {

    Assets::GlobalMaterialRegistry::enumerate(
        [&]( StringView name, Assets::MaterialArchetypeInfo info ) {
            AXION_UNUSED_PARAMETER( name );
            _mtlLib.registerArchetype( info.name, info.shaderModule, info.shaderSpcecializationType );
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
    _passes.registerPass<VisPass>();
    // TBD
    //  _passes.registerPass<BinningPass>();
    _passes.registerPass<VisResolvePass>();
    _passes.registerPass<ToneMappingPass>();
    _passes.registerPass<FXAAPass>();
}

void Rasterizer::createResources() {

    auto& r = _rnd->resources();

    //------------------------- A. GLOBAL BUFFERS (Persistent) -------------------
    const u64 vertexBufferSize = ( _settings.memory.device.maxGeometryAlloc * 75 ) / 100;
    const u64 indexBufferSize  = _settings.memory.device.maxGeometryAlloc - vertexBufferSize;

    _res.vertexBufferHandle = r.buffer( "GlobalVertexBuffer" )
                                  .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                  .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                  .size( vertexBufferSize )
                                  .onGPU()
                                  .asRaw()
                                  .create();
    _res.vertexAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( _res.vertexBufferHandle ) );

    _res.indexBufferHandle = r.buffer( "GlobalIndexBuffer" )
                                 .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                 .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                 .size( indexBufferSize )
                                 .onGPU()
                                 .asRaw()
                                 .asIBO()
                                 .create();
    _res.indexAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( _res.indexBufferHandle ) );

    _res.mtlBufferHandle = r.buffer( "GlobalMaterialBuffer" )
                               .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                               .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                               .size( _settings.memory.device.maxMaterialAlloc )
                               .onGPU()
                               .asRaw()
                               .create();
    _res.mtlAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( _res.mtlBufferHandle ) );

    //------------------------- B. Fallback Resources -------------------

    STLW::Array<byte, 4> fallbackPixels = { 255, 0, 255, 255 };
    _res.fallbackTexture2DHandle        = r.texture( "FallbackTexture2D" )
                                       .format( Graphics::Format::RGBA8_UNORM )
                                       .extent( { 1, 1, 1 } )
                                       .withData( fallbackPixels.data() )
                                       .create();
    _res.fallbackTexture3DHandle = r.texture( "FallbackTexture3D" )
                                       .format( Graphics::Format::RGBA8_UNORM )
                                       .extent( { 1, 1, 1 } )
                                       .dim( Graphics::TextureDimension::Texture3D )
                                       .withData( fallbackPixels.data() )
                                       .create();
    _res.fallbackTextureCubeHandle = r.texture( "FallbackTextureCube" )
                                         .format( Graphics::Format::RGBA8_UNORM )
                                         .extent( { 1, 1, 1 } )
                                         .asCubeMap()
                                         .withData( fallbackPixels.data() )
                                         .create();

    _res.fallbackSamplerHandle = r.sampler( "FallbackSampler" ).create();

    STLW::Vector<Graphics::RHI::ITexture*> initial2DTextures( Config::MAX_PERSISTENT_2D_TEXTURES, r.getTexture( _res.fallbackTexture2DHandle ) );
    STLW::Vector<Graphics::RHI::ITexture*> initial3DTextures( Config::MAX_PERSISTENT_3D_TEXTURES, r.getTexture( _res.fallbackTexture3DHandle ) );
    STLW::Vector<Graphics::RHI::ITexture*> initialCubeTextures( Config::MAX_PERSISTENT_CUBE_TEXTURES, r.getTexture( _res.fallbackTextureCubeHandle ) );
    STLW::Vector<Graphics::RHI::ISampler*> initialSamplers( Config::MAX_PERSISTENT_SAMPLERS, r.getSampler( _res.fallbackSamplerHandle ) );

    // ----------------------- C. PER-FRAME BUFFERS (Volatile) -------------------

    _res.frame.resize( _FRAMES_IN_FLIGHT );
    for ( u32 i = 0; i < _FRAMES_IN_FLIGHT; ++i )
    {
        _res.frame[i].uboBufferHandle = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                            .size( Config::MAX_GLOBAL_UBO_BYTES )
                                            .onCPU()
                                            .create();
        auto* uboBuffer = r.getBuffer( _res.frame[i].uboBufferHandle );
        uboBuffer->map();
        _res.frame[i].uboAllocator = Graphics::BufferLinearAllocator<>( uboBuffer );

        _res.frame[i].ssboBufferHandle = r.buffer( "GlobalSSBO_" + std::to_string( i ) )
                                             .size( _settings.memory.shared.maxConstantAllocPerFrame )
                                             .onCPU()
                                             .create();

        auto* ssboBuffer = r.getBuffer( _res.frame[i].ssboBufferHandle );
        ssboBuffer->map();
        _res.frame[i].ssboAllocator = Graphics::BufferLinearAllocator<>( ssboBuffer );

        // ----------------------- D. PER-FRAME INDIRECT RENDERING BUFFERS (Volatile) -------------------
        _res.frame[i].indirectStagingBufferHandle = r.buffer( "IndirectStagingCommandBuffer_" + std::to_string( i ) )
                                                        .size( _settings.memory.shared.maxExecutableAlloc )
                                                        .onCPU()
                                                        .create();
        auto* indBuffer = r.getBuffer( _res.frame[i].indirectStagingBufferHandle );
        indBuffer->map();
        _res.frame[i].indirectAllocator = Graphics::BufferLinearAllocator<>( indBuffer );

        _res.frame[i].indirectBufferHandle = r.buffer( "IndirectCommandBuffer_" + std::to_string( i ) )
                                                 .size( _settings.memory.shared.maxExecutableAlloc )
                                                 .asSSBO()
                                                 .stride( sizeof( Graphics::RHI::DrawIndexedIndirectCommand ) )
                                                 .create();
        _res.frame[i].indirectTemplateBufferHandle = r.buffer( "IndirectTemplateCommandBuffer_" + std::to_string( i ) )
                                                         .size( _settings.memory.shared.maxExecutableAlloc )
                                                         .asSSBO()
                                                         .stride( sizeof( Graphics::RHI::DrawIndexedIndirectCommand ) )
                                                         .create();

        _res.frame[i].culledInstanceBufferHandle = r.buffer( "IndirectCulledInstanceBuffer_" + std::to_string( i ) )
                                                       .size( _settings.memory.shared.maxConstantAllocPerFrame )
                                                       .stride( sizeof( u32 ) )
                                                       .onGPU()
                                                       .asSSBO()
                                                       .create();

        // ----------------------- E. PER-FRAME DESCRIPTOR SET (Persistent) -------------------
        Graphics::RHI::IDescriptorAllocator* frameDescriptorAllocator = _rnd->getFrameDescriptorAllocator( i );
        Graphics::RHI::IDescriptorSet*       persistentSet            = frameDescriptorAllocator->allocate( _rnd->pipelines().getLayout( _globalMtlLayoutHandle ), (u32)Config::DescriptorSetFrequency::Persistent );

        // Attach core persistent buffers
        persistentSet->attach( 0, Graphics::RHI::DescriptorType::SRV_Buffer, r.getBuffer( _res.vertexBufferHandle ) );
        persistentSet->attach( 1, Graphics::RHI::DescriptorType::SRV_Buffer, r.getBuffer( _res.indexBufferHandle ) );
        persistentSet->attach( 2, Graphics::RHI::DescriptorType::SRV_Buffer, r.getBuffer( _res.mtlBufferHandle ) );
        persistentSet->attachBindlessArray( 3, 0, Graphics::RHI::DescriptorType::SRV_Image, initial2DTextures );
        persistentSet->attachBindlessArray( 4, 0, Graphics::RHI::DescriptorType::SRV_Image, initial3DTextures );
        persistentSet->attachBindlessArray( 5, 0, Graphics::RHI::DescriptorType::SRV_Image, initialCubeTextures );
        persistentSet->attachBindlessArray( 0, 0, initialSamplers );

        frameDescriptorAllocator->lockPersistent();

        // Store Persistent Descriptor Set Ptr
        _res.frame[i].persistentDescriptorSetPtr = persistentSet;
    }
}

#pragma endregion
#pragma region CPU-GPU Uploads

Rasterizer::TransientPayload Rasterizer::uploadTransientData( Graphics::BufferLinearAllocator<>& currentUBOAlloc,
                                                              Graphics::BufferLinearAllocator<>& currentSSBOAlloc ) {
    TransientPayload payload;

    // (D3D12/Vulkan)
    const u32 CBV_ALIGNMENT = 256;

    // =================================================================================
    // 1. FRAME DATA (UBO - Constant Buffer)
    // =================================================================================
    {
        const auto& frameData = _gpuScene.frame();

        payload.frameSlice = currentUBOAlloc.allocate( sizeof( GPUFrame ), CBV_ALIGNMENT );

        if ( payload.frameSlice.isValid() )
        {
            memcpy( payload.frameSlice.cpuAddress, &frameData, sizeof( GPUFrame ) );
            payload.frameSlice.stride = sizeof( GPUFrame );
            payload.frameSlice.count  = 1;
        }
    }

    // =================================================================================
    // 2. MESH DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& meshes = _gpuScene.meshes();

        if ( !meshes.empty() )
        {
            payload.meshesSlice = currentSSBOAlloc.allocate<GPUMesh>( meshes.size() );

            if ( payload.meshesSlice.isValid() )
            {
                memcpy( payload.meshesSlice.cpuAddress, meshes.data(), payload.meshesSlice.size );
            }
        } else
        {
            // Dummy
            payload.meshesSlice       = currentSSBOAlloc.allocate<GPUMesh>( 1 );
            payload.meshesSlice.count = 0; // Empty
            if ( payload.meshesSlice.cpuAddress )
                memset( payload.meshesSlice.cpuAddress, 0, payload.meshesSlice.size );
        }
    }

    // =================================================================================
    // 3. MATERIAL DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& mtls = _gpuScene.materials();

        if ( !mtls.empty() )
        {
            payload.mtlSlice = currentSSBOAlloc.allocate<GPUMaterial>( mtls.size() );

            if ( payload.mtlSlice.isValid() )
            {
                memcpy( payload.mtlSlice.cpuAddress, mtls.data(), payload.mtlSlice.size );
            }
        } else
        {
            // DUMMY
            payload.mtlSlice       = currentSSBOAlloc.allocate<GPUMaterial>( 1 );
            payload.mtlSlice.count = 0;
            if ( payload.mtlSlice.cpuAddress )
                memset( payload.mtlSlice.cpuAddress, 0, payload.mtlSlice.size );
        }
    }
    // =================================================================================
    // 4. INSTANCE DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& instances = _gpuScene.instances();

        if ( !instances.isEmpty() )
        {
            payload.instancesSlice = currentSSBOAlloc.allocate<GPUInstance>( instances.size() );

            if ( payload.instancesSlice.isValid() )
            {
                memcpy( payload.instancesSlice.cpuAddress, instances.data(), payload.instancesSlice.size );
            }
        } else
        {
            // DUMMY
            payload.instancesSlice       = currentSSBOAlloc.allocate<GPUInstance>( 1 );
            payload.instancesSlice.count = 0;
            if ( payload.instancesSlice.cpuAddress )
                memset( payload.instancesSlice.cpuAddress, 0, payload.instancesSlice.size );
        }
    }

    // =================================================================================
    // 5. LIGHT DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& lights = _gpuScene.lights();

        if ( !lights.isEmpty() )
        {
            payload.lightsSlice = currentSSBOAlloc.allocate<GPULight>( lights.size() );

            if ( payload.lightsSlice.isValid() )
            {
                memcpy( payload.lightsSlice.cpuAddress, lights.data(), payload.lightsSlice.size );
            }
        } else
        {
            // DUMMY
            payload.lightsSlice       = currentSSBOAlloc.allocate<GPULight>( 1 );
            payload.lightsSlice.count = 0;
            if ( payload.lightsSlice.cpuAddress )
                memset( payload.lightsSlice.cpuAddress, 0, payload.lightsSlice.size );
        }
    }
    // =================================================================================
    // 6. ENV DATA (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& envs = _gpuScene.environments();

        if ( !envs.isEmpty() )
        {
            payload.envsSlice = currentSSBOAlloc.allocate<GPUEnvironment>( envs.size() );

            if ( payload.envsSlice.isValid() )
            {
                memcpy( payload.envsSlice.cpuAddress, envs.data(), payload.envsSlice.size );
            }
        } else
        {
            // DUMMY
            payload.envsSlice       = currentSSBOAlloc.allocate<GPUEnvironment>( 1 );
            payload.envsSlice.count = 0;
            if ( payload.envsSlice.cpuAddress )
                memset( payload.envsSlice.cpuAddress, 0, payload.envsSlice.size );
        }
    }

    // =================================================================================
    // 7. INSTANCE REDIRECTION DATA (for true instancing) (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& sortedKeys = _gpuScene.getSortedKeys();

        if ( !sortedKeys.isEmpty() )
        {
            payload.redirectSlice = currentSSBOAlloc.allocate<u32>( sortedKeys.size() );

            if ( payload.redirectSlice.isValid() )
            {
                auto* redirectPtr = (u32*)payload.redirectSlice.cpuAddress;

                for ( size_t i = 0; i < sortedKeys.size(); ++i )
                {
                    redirectPtr[i] = sortedKeys[i].originalInstanceIdx;
                }
            }
        } else
        {
            payload.redirectSlice       = currentSSBOAlloc.allocate<u32>( 1 );
            payload.redirectSlice.count = 0;
            if ( payload.redirectSlice.cpuAddress )
                memset( payload.redirectSlice.cpuAddress, 0, payload.redirectSlice.size );
        }
    }

    return payload;
}

IndirectCommandPayload Rasterizer::uploadIndirectCommandData(
    Graphics::BufferLinearAllocator<>& currentSSBOAlloc,
    Graphics::BufferLinearAllocator<>& currentIndirectAlloc ) {
    const auto& sortedKeys = _gpuScene.getSortedKeys();
    const auto& instances  = _gpuScene.instances();
    const auto& meshes     = _gpuScene.meshes();

    IndirectCommandPayload payload;
    if ( sortedKeys.isEmpty() )
        return payload;

    // 1. ALLOCATIONS
    // =================================================================================

    size_t maxPossibleCommands = meshes.size() * _mtlLib.getArchetypesCount();
    maxPossibleCommands        = std::max<size_t>( maxPossibleCommands, 1 );

    auto cmdAlloc = currentIndirectAlloc.allocate<Graphics::RHI::DrawIndexedIndirectCommand>( maxPossibleCommands );

    if ( !cmdAlloc.isValid() )
        return payload;

    auto*      cmdPtr       = (Graphics::RHI::DrawIndexedIndirectCommand*)cmdAlloc.cpuAddress;
    const auto RHI_CMD_SIZE = sizeof( Graphics::RHI::DrawIndexedIndirectCommand );

    payload.batches.reserve( _mtlLib.getArchetypesCount() ); // Optimistic

    // 2. LOOP
    // =================================================================================

    u32 currentPso = 0, currentMeshID = 0;
    sortedKeys[0].unpack( currentPso, currentMeshID );

    auto lastMesh = meshes[instances[sortedKeys[0].originalInstanceIdx].meshID];

    // Trackers
    u32 batchStartOffsetInRedirect = 0;
    u32 instanceAccumulator        = 0;

    u32 cmdWriteIdx          = 0;
    u32 cmdsInCurrentArch    = 0;
    u32 archBatchStartCmdIdx = 0;

    u32*                  indirectCmdMapPtr = nullptr;
    Graphics::BufferSlice cmdMapAlloc {};
    // bool                  useGPUCulling = _settings.common.flags & RendererEnableGPUCulling;
    bool useGPUCulling = false;
    if ( useGPUCulling )
    {
        cmdMapAlloc       = currentSSBOAlloc.allocate<u32>( instances.size() );
        indirectCmdMapPtr = (u32*)cmdMapAlloc.cpuAddress;
    }

    for ( size_t i = 0; i < sortedKeys.size(); ++i )
    {
        u32 psoID, meshID;
        sortedKeys[i].unpack( psoID, meshID );

        bool breakInstancing = ( meshID != currentMeshID ) ||
                               ( psoID != currentPso );

        if ( breakInstancing && instanceAccumulator > 0 )
        {
            Graphics::RHI::DrawIndexedIndirectCommand cmd;
            cmd.indexCount    = lastMesh.indexCount;
            cmd.instanceCount = useGPUCulling ? 0 : instanceAccumulator;
            cmd.firstIndex    = lastMesh.indexOffset / 4;
            cmd.vertexOffset  = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
            cmd.firstInstance = 0;

            cmd.baseInstanceID = batchStartOffsetInRedirect;

            cmdPtr[cmdWriteIdx] = cmd;

            cmdWriteIdx++;
            cmdsInCurrentArch++;

            // Reset
            instanceAccumulator        = 0;
            batchStartOffsetInRedirect = (u32)i;

            // Update Trackers
            currentMeshID   = meshID;
            u32 originalIdx = sortedKeys[i].originalInstanceIdx;
            lastMesh        = meshes[instances[originalIdx].meshID];
        }

        if ( psoID != currentPso )
        {
            payload.batches.push_back( { .psoID        = currentPso,
                                         .bufferOffset = (u32)( cmdAlloc.offset + ( archBatchStartCmdIdx * RHI_CMD_SIZE ) ),
                                         .drawCount    = cmdsInCurrentArch } );

            currentPso           = psoID;
            cmdsInCurrentArch    = 0;
            archBatchStartCmdIdx = cmdWriteIdx;
        }

        if ( useGPUCulling )
            indirectCmdMapPtr[i] = cmdWriteIdx;

        instanceAccumulator++;
    }

    // 3. Close
    // =================================================================================

    if ( instanceAccumulator > 0 )
    {
        Graphics::RHI::DrawIndexedIndirectCommand cmd;
        cmd.indexCount    = lastMesh.indexCount;
        cmd.instanceCount = useGPUCulling ? 0 : instanceAccumulator;
        cmd.firstIndex    = lastMesh.indexOffset / 4;
        cmd.vertexOffset  = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
        cmd.firstInstance = 0;

        cmd.baseInstanceID = batchStartOffsetInRedirect;

        cmdPtr[cmdWriteIdx++] = cmd;
        cmdsInCurrentArch++;
    }

    // Last Render Batch
    if ( cmdsInCurrentArch > 0 )
    {
        payload.batches.push_back( { .psoID        = currentPso,
                                     .bufferOffset = (u32)( cmdAlloc.offset + ( archBatchStartCmdIdx * RHI_CMD_SIZE ) ),
                                     .drawCount    = cmdsInCurrentArch } );
    }

    // 4. Finish
    // =================================================================================

    payload.commandBufferSlice      = cmdAlloc;
    payload.commandBufferSlice.size = cmdWriteIdx * RHI_CMD_SIZE;
    payload.batchMapSlice           = cmdMapAlloc;

    if ( !useGPUCulling )
        return payload;

#ifdef AXION_DEBUG
    payload.dirty = true;

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
        else if ( std::memcmp( indirectCmdMapPtr, _indirectCommandDataCache.batchMap.data(), currentMapCount * sizeof( u32 ) ) != 0 )
            structuralChanges = true;
    }

    if ( structuralChanges )
    {
        _indirectCommandDataCache.commands.assign( cmdPtr, cmdPtr + currentCmdCount );

        _indirectCommandDataCache.batchMap.assign( indirectCmdMapPtr, indirectCmdMapPtr + currentMapCount );

        //  CPU->GPU
        payload.dirty = true;
    } else
    {
        // CPU->GPU
        payload.dirty = false;
    }

#endif

    return payload;
}

#pragma endregion
#pragma region Misc

Graphics::IRenderer::MemoryBudget Rasterizer::convertMemoryBudget() {
    Graphics::IRenderer::MemoryBudget lowLevelBudget;

    // 1. DEVICE POOLS (Pure VRAM)
    lowLevelBudget.device.maxTextureAlloc      = _settings.memory.device.maxTextureAlloc;
    lowLevelBudget.device.maxRenderTargetAlloc = _settings.memory.device.maxRenderTargetAlloc;

    u64 totalGPUBufferSize =
        _settings.memory.device.maxGeometryAlloc + // GlobalVertexBuffer & GlobalIndexBuffer
        _settings.memory.device.maxMaterialAlloc;  // GlobalMaterialBuffer

    // Add volatile GPU-side buffers that exist per-frame (like Indirect/Culling targets)
    u64 perFrameGPUBufferSize =
        ( _settings.memory.shared.maxExecutableAlloc * 50 ) +                                              // indirectBufferHandle & indirectTemplateBufferHandle
        _settings.memory.shared.maxConstantAllocPerFrame + _settings.memory.shared.maxUploadAllocPerFrame; // culledInstanceBufferHandle

    totalGPUBufferSize += ( perFrameGPUBufferSize * _FRAMES_IN_FLIGHT );

    lowLevelBudget.device.maxBufferAlloc = totalGPUBufferSize;

    // 2. UPLOAD / CPU-VISIBLE POOLS (Mapped RAM)

    // Calculate total mapped memory required across all frames in flight
    u64 perFrameMappedBufferSize =
        Config::MAX_GLOBAL_UBO_BYTES +                  // uboBufferHandle
        _settings.memory.shared.maxExecutableAlloc +    // indirectStagingBufferHandle
        _settings.memory.shared.maxUploadAllocPerFrame; // general transient upload

    lowLevelBudget.device.maxUploadAlloc = perFrameMappedBufferSize * _FRAMES_IN_FLIGHT;

    // 3. HOST MEMORY (Standard CPU RAM)
    // -------------------------------------------------------------------------

    lowLevelBudget.host.maxPersistentAlloc        = _settings.memory.host.maxPersistentAlloc / 2;
    lowLevelBudget.host.maxTransientAllocPerFrame = _settings.memory.host.maxTransientAllocPerFrame / 2;

    lowLevelBudget.device.strict = _settings.memory.strictVRAM;

    return lowLevelBudget;
}

} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END