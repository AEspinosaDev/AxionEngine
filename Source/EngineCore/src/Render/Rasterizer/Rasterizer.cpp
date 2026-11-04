
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

    auto lowLevelMemoryBudget = MemoryBudget::toLowLevelMemoryBudget( _settings.memory, _FRAMES_IN_FLIGHT );

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

    Graphics::RendererDescriptorBudget descBudget = {
        .maxDescriptorsPerFrame = 2048,
        .maxViewsPerFrame       = Config::MAX_SHADER_RESOURCE_VIEWS,
        .maxSamplersPerFrame    = Config::MAX_SAMPLER_VIEWS,
    };

    Graphics::RendererSettings rndStts = {
        .gfxApi           = settings.common.gfxApi,
        .bufferingType    = settings.common.bufferingType,
        .debugMode        = ( settings.common.flags & RendererEnableDebug ) != RendererNone,
        .presentMode      = wnd->getSettings().flags & Platform::WindowVSync ? Graphics::PresentMode::Vsync : Graphics::PresentMode::Immediate,
        .backbufferFormat = settings.common.backbufferFormat,
        .memory           = lowLevelMemoryBudget,
        .descriptorBudget = descBudget,
        .maxSBTAlloc      = settings.memory.shared.maxExecutableAlloc,
        .maxStagingAlloc  = settings.memory.shared.maxUploadAllocPerFrame,
        .maxScratchAlloc  = settings.memory.shared.maxUploadAllocPerFrame,
        .RDGmaxAlloc      = KBYTES( 1024 ),
        .GCMode           = settings.common.GCMode,
        .autoSync         = true,
        .enableTimeQueries = ( settings.common.flags & RendererEnableGPUTimeQueries ) != RendererNone,
        .selectedDeviceID = settings.common.selectedDeviceID,
        .enableGui        = ( settings.common.flags & RendererEnableGUI ) != RendererNone };

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

    // Bindless defaults read by shaders through gFrame
    _gpuScene.frame().fallbackTex2DID = _rnd->resources().getTexture( _res.fallbackTexture2DHandle )->getBindlessSRVIdx();
    _gpuScene.frame().linearSamplerID  = _rnd->resources().getSampler( _res.fallbackSamplerHandle )->getBindlessIdx();
    _gpuScene.frame().materialBufferID = _rnd->resources().getBuffer( _res.mtlBufferHandle )->getBindlessSRVIdx();

    _res.materialTextureHandles.resize( _gpuScene.textures().size() );

    // Material Library Update
    _mtlLib.updatePipelines( _rnd->pipelines() );

    // Reset allocators
    const u32 FRAME_ID        = _rnd->getCurrentFrameIndex();
    auto&     currentFrameRes = _res.frame[FRAME_ID];
    currentFrameRes.uboAllocator.reset();
    currentFrameRes.ssboAllocator.reset();
    currentFrameRes.indirectAllocator.reset();

    // Geometry residency: pending meshes are placed in the geometry pages before GPUMesh is uploaded (heap IDs & offsets valid this frame)
    auto meshUploads = _passes.getPass<UploadPass>()->reserveMeshes( _gpuScene,
                                                                     _res,
                                                                     _rnd->resources(),
                                                                     _settings.memory.device.minGeometryPageAlloc,
                                                                     _settings.memory.shared.maxUploadAllocPerFrame );

    // Upload CPU Coherent Data to GPU
    auto transientPayload   = uploadTransientData( currentFrameRes.uboAllocator,
                                                 currentFrameRes.ssboAllocator );
    auto indirectCmdPayload = uploadIndirectCommandData( currentFrameRes.ssboAllocator,
                                                         currentFrameRes.indirectAllocator );

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {

        const Extent3D SCENE_RESOLUTION  = _window->getSettings().size.to3D();
        const Extent3D SCREEN_RESOLUTION = _window->getSettings().size.to3D();

        const bool GPU_CULLING_ENABLED = ( _settings.common.flags & RendererEnableGPUCulling ) && !indirectCmdPayload.batches.empty();

        //----------------------------
        // 0. Allocate Frame Descriptor Set
        //----------------------------
        Graphics::RHI::IDescriptorSet* transientSetPtr = builder.allocateDescriptorSet(_rnd->pipelines().getLayout( _globalMtlLayoutHandle ), (u32)Config::LayoutSetType::FrameBound );

        auto frameSRV = []( Config::FrameSRV s ) { return Config::REG_FRAME_SRV_BASE + (u32)s; };

        transientSetPtr->attachBufferSlice( Config::REG_FRAME_CBV_BASE, Graphics::RHI::DescriptorType::CBV, transientPayload.frameSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Meshes ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.meshesSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Materials ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.mtlSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Instances ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.instancesSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Lights ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.lightsSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Envs ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.envsSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::Textures ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.texturesSlice );
        transientSetPtr->attachBufferSlice( frameSRV( Config::FrameSRV::InstanceRedirection ), Graphics::RHI::DescriptorType::SRV_Buffer, transientPayload.redirectSlice ); // Always the full sorted list

        //----------------------------
        // A. Upload Global Data
        //----------------------------
        UploadPass::Config upConfig;
        upConfig.outGlobalBufferHandles = Resources::importGlobalBuffers( builder, _res ); // Every page (including the ones created this frame)
        upConfig.gpuScene               = &_gpuScene;
        upConfig.meshUploads            = std::move( meshUploads );
        upConfig.matAllocator           = &_res.mtlAllocator;
        upConfig.maxAllocationSize      = _settings.memory.shared.maxUploadAllocPerFrame;

        upConfig.mtlTextureHandles = &_res.materialTextureHandles;

        _passes.getPass<UploadPass>()->addToGraph( builder, upConfig );

        //----------------------------
        // B. GPU-Culling
        //----------------------------
        CullingPass::Config cullConfig;
        if ( GPU_CULLING_ENABLED )
        {
            IndirectUploadPass::Config indUpConfig;
            indUpConfig.inOutIndirectBufferHandle = builder.import( "IndirectCommandBuffer", currentFrameRes.indirectBufferHandle );
            indUpConfig.indirectData              = indirectCmdPayload;

            _passes.getPass<IndirectUploadPass>()->addToGraph( builder, indUpConfig );

            cullConfig.outIndirectBufferHandle       = indUpConfig.inOutIndirectBufferHandle;
            cullConfig.outCulledRedirectBufferHandle = builder.import( "IndirectCulledInstanceBuffer", currentFrameRes.culledInstanceBufferHandle );

            cullConfig.inCommandMapSlice      = indirectCmdPayload.batchMapSlice;
            cullConfig.transientDescriptorSet = transientSetPtr;
            cullConfig.instanceCount          = (u32)_gpuScene.getSortedKeys().size(); // Drawable instances only (invalid bundles are not sorted)

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

        visConfig.inGlobalBufferHandles = upConfig.outGlobalBufferHandles;
        visConfig.matLib                = &_mtlLib;
        visConfig.materialPassSlot = (u32)Config::MaterialPassType::Visibility;

        visConfig.indirectData                 = indirectCmdPayload;
        visConfig.inIndirectBufferHandle       = cullConfig.outIndirectBufferHandle;
        visConfig.inCulledRedirectBufferHandle = cullConfig.outCulledRedirectBufferHandle;
        visConfig.inInstanceListSlice          = transientPayload.redirectSlice;
        visConfig.useGPUCulling                = GPU_CULLING_ENABLED;

        visConfig.transientDescriptorSet = transientSetPtr;

        _passes.getPass<VisPass>()->addToGraph( builder, visConfig );

        //----------------------------
        // D. Resolve Vis
        //----------------------------
        VisResolvePass::Config resConfig;
        resConfig.outColorHandle = builder.texture( "ColorRTO" )
                                       .asStorage() // Compute UAV output only (an RT flag would require a clear before first use)
                                       .format( Graphics::Format::RGBA16_FLOAT )
                                       .extent( SCENE_RESOLUTION )
                                       .create();
        resConfig.inVisHandle           = visConfig.outVisHandle;
        resConfig.inGlobalBufferHandles = upConfig.outGlobalBufferHandles;
        resConfig.indirectData          = indirectCmdPayload; // One dispatch per material bundle

        resConfig.ioSetId = (u32)Config::LayoutSetType::PassBound;

        resConfig.materialPassSlot = (u32)Config::MaterialPassType::VisibilityResolve;
        resConfig.matLib           = &_mtlLib;

        resConfig.transientDescriptorSet = transientSetPtr;

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

    // Every pass works on the global layout contract (before createPipelines)
    _passes.setGlobalLayout( _globalMtlLayoutHandle );
}

void Rasterizer::createResources() {

    auto& r = _rnd->resources();

    // Material buffer + fallbacks
    Resources::createPersistentResources( r, _settings.memory, _res );
    // First geometry page (the rest are created on demand by the UploadPass)
    Resources::createGeometryPage( r, Resources::GeometryPageSizes::fromBudget( _settings.memory.device.minGeometryPageAlloc ), _res );
    // Per frame in flight buffers
    Resources::createFrameResources( r, _settings.memory, _FRAMES_IN_FLIGHT, _res );
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

    // =================================================================================
    // 8. TEXTURE DATA (GPUScene texture cache -> slot to bindless heap index) (SSBO - Structured Buffer)
    // =================================================================================
    {
        const auto& textures = _gpuScene.textures();

        if ( !textures.empty() )
        {
            payload.texturesSlice = currentSSBOAlloc.allocate<GPUTexture>( textures.size() );

            if ( payload.texturesSlice.isValid() )
            {
                memcpy( payload.texturesSlice.cpuAddress, textures.data(), payload.texturesSlice.size );
            }
        } else
        {
            // DUMMY
            payload.texturesSlice       = currentSSBOAlloc.allocate<GPUTexture>( 1 );
            payload.texturesSlice.count = 0;
            if ( payload.texturesSlice.cpuAddress )
                memset( payload.texturesSlice.cpuAddress, 0, payload.texturesSlice.size );
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

    // One command per (pso bundle, mesh), plus the splits of instance runs bigger than a single DispatchMesh
    constexpr u32 MIN_INSTANCES_PER_COMMAND = Config::MAX_MESH_DISPATCH_GROUPS / Config::MAX_MESH_DISPATCH_GROUPS_PER_DIMENSION;
    size_t        maxPossibleCommands       = meshes.size() * _mtlLib.getArchetypesCount() + sortedKeys.size() / MIN_INSTANCES_PER_COMMAND;
    maxPossibleCommands                     = std::max<size_t>( maxPossibleCommands, 1 );

    auto cmdAlloc = currentIndirectAlloc.allocate<Graphics::RHI::DispatchMeshIndirectCommand>( maxPossibleCommands );

    if ( !cmdAlloc.isValid() )
        return payload;

    auto*      cmdPtr       = (Graphics::RHI::DispatchMeshIndirectCommand*)cmdAlloc.cpuAddress;
    const auto RHI_CMD_SIZE = sizeof( Graphics::RHI::DispatchMeshIndirectCommand );

    // Amplification groups per instance: each one culls a chunk of meshlets
    auto meshletChunks = []( u32 meshletCount ) { return ( meshletCount + Config::VIS_AMPLIFICATION_GROUP_SIZE - 1 ) / Config::VIS_AMPLIFICATION_GROUP_SIZE; };
    // Meshes not placed in a geometry page yet (upload budget) dispatch nothing: their heap IDs are invalid
    auto residentMeshletChunks = [&]( const GPUMesh& mesh ) { return mesh.geometryPageID != AXION_INVALID_U32 ? meshletChunks( mesh.meshletCount ) : 0u; };

    // DispatchMesh grid = ( meshlet chunks, instances, 1 ) is limited per dimension and in total groups
    auto maxInstancesPerCommand = [&]( u32 meshletCount ) {
        const u32 chunks = meshletChunks( meshletCount );
        return chunks == 0 ? Config::MAX_MESH_DISPATCH_GROUPS_PER_DIMENSION
                           : std::min( Config::MAX_MESH_DISPATCH_GROUPS_PER_DIMENSION, Config::MAX_MESH_DISPATCH_GROUPS / chunks );
    };

    payload.batches.reserve( _mtlLib.getArchetypesCount() ); // Optimistic

    // 2. LOOP
    // =================================================================================

    u32 currentPsoBundle = 0, currentMeshID = 0;
    sortedKeys[0].unpack( currentPsoBundle, currentMeshID );

    auto lastMesh = meshes[instances[sortedKeys[0].originalInstanceIdx].meshID];

    // Trackers
    u32 batchStartOffsetInRedirect = 0;
    u32 instanceAccumulator        = 0;

    u32 cmdWriteIdx          = 0;
    u32 cmdsInCurrentArch    = 0;
    u32 archBatchStartCmdIdx = 0;

    u32*                  indirectCmdMapPtr = nullptr;
    Graphics::BufferSlice cmdMapAlloc {};
    const bool useGPUCulling = _settings.common.flags & RendererEnableGPUCulling;
    if ( useGPUCulling )
    {
        cmdMapAlloc       = currentSSBOAlloc.allocate<u32>( sortedKeys.size() );
        indirectCmdMapPtr = (u32*)cmdMapAlloc.cpuAddress;
    }

    for ( size_t i = 0; i < sortedKeys.size(); ++i )
    {
        u32 psoBundleID, meshID;
        sortedKeys[i].unpack( psoBundleID, meshID );

        bool breakInstancing = ( meshID != currentMeshID ) ||
                               ( psoBundleID != currentPsoBundle ) ||
                               ( instanceAccumulator == maxInstancesPerCommand( lastMesh.meshletCount ) );

        if ( breakInstancing && instanceAccumulator > 0 )
        {
            Graphics::RHI::DispatchMeshIndirectCommand cmd;
            cmd.baseInstanceID    = batchStartOffsetInRedirect;
            cmd.threadGroupCountX = residentMeshletChunks( lastMesh );
            cmd.threadGroupCountY = useGPUCulling ? 0 : instanceAccumulator; // GPU culling fills it with the visible ones
            cmd.threadGroupCountZ = 1;

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

        if ( psoBundleID != currentPsoBundle )
        {
            payload.batches.push_back( { .psoBundleID  = currentPsoBundle,
                                         .bufferOffset = (u32)( archBatchStartCmdIdx * RHI_CMD_SIZE ),
                                         .drawCount    = cmdsInCurrentArch } );

            currentPsoBundle     = psoBundleID;
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
        Graphics::RHI::DispatchMeshIndirectCommand cmd;
        cmd.baseInstanceID    = batchStartOffsetInRedirect;
        cmd.threadGroupCountX = residentMeshletChunks( lastMesh );
        cmd.threadGroupCountY = useGPUCulling ? 0 : instanceAccumulator; // GPU culling fills it with the visible ones
        cmd.threadGroupCountZ = 1;

        cmdPtr[cmdWriteIdx++] = cmd;
        cmdsInCurrentArch++;
    }

    // Last Render Batch
    if ( cmdsInCurrentArch > 0 )
    {
        payload.batches.push_back( { .psoBundleID  = currentPsoBundle,
                                     .bufferOffset = (u32)( archBatchStartCmdIdx * RHI_CMD_SIZE ),
                                     .drawCount    = cmdsInCurrentArch } );
    }

    // 4. Finish
    // =================================================================================

    payload.commandBufferSlice      = cmdAlloc;
    payload.commandBufferSlice.size = cmdWriteIdx * RHI_CMD_SIZE;
    payload.batchMapSlice           = cmdMapAlloc;

    return payload;
}

#pragma endregion
#pragma region Misc

} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END