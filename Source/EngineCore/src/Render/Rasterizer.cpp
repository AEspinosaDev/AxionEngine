
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
        .RGTransientAllocSize  = settings.memory.uploadBufferSize,
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

    GPUSceneUpdateFlags updateFlags = GPUSceneSortInstances;
    if ( _settings.common.gfxApi == Graphics::API::DirectX12 )
        updateFlags |= GPUSceneTransposeMatrices;

    _gpuScene.update( scene,
                      cameraEntity,
                      _mtlLib,
                      _window->getSize(),
                      deltaTime,
                      updateFlags );

    auto& currentFrameRes = _res.frame[_rnd->getCurrentFrameIndex()];
    currentFrameRes.uboAllocator.reset();
    currentFrameRes.ssboAllocator.reset();
    currentFrameRes.indirectAllocator.reset();

    auto transientViews  = uploadTransientData( currentFrameRes.uboAllocator,
                                               currentFrameRes.ssboAllocator );
    auto indirectCmdData = uploadIndirectCommands( currentFrameRes.indirectAllocator );

    _rnd->render( [&]( Axion::Graphics::RenderGraphBuilder& builder ) {
        auto rtExtent = _window->getSettings().size.to3D();

        // A. Upload
        UploadPass::Config upConfig;
        upConfig.bufferHandles = {
            .vertex    = builder.import( "GlobalVertexBuffer", _res.vertexBufferHandle ),
            .index     = builder.import( "GlobalIndexBuffer", _res.indexBufferHandle ),
            .materials = builder.import( "GlobalMaterialBuffer", _res.mtlBufferHandle ) };
        upConfig.gpuScene          = &_gpuScene;
        upConfig.vertexAllocator   = &_res.vertexAllocator;
        upConfig.indexAllocator    = &_res.indexAllocator;
        upConfig.matAllocator      = &_res.mtlAllocator;
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
            .vertex   = upConfig.bufferHandles.vertex,
            .index    = upConfig.bufferHandles.index,
            .material = upConfig.bufferHandles.materials },
        fwConfig.matLib          = &_mtlLib;
        fwConfig.matLayoutHandle = _globalMtlLayoutHandle;
        fwConfig.gpuScene        = &_gpuScene;

        fwConfig.frameView       = transientViews.frameView;
        fwConfig.meshesView      = transientViews.meshesView;
        fwConfig.materialsView   = transientViews.mtlView;
        fwConfig.instancesView   = transientViews.instancesView;
        fwConfig.lightsView      = transientViews.lightsView;
        fwConfig.redirectionView = transientViews.redirectView;

        fwConfig.indirectData = indirectCmdData;

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

    // Global Layout (BINDLESS)
    _globalMtlLayoutHandle = _rnd->pipelines().layout( "Global_Material_Layout" )
                                 // Space 0: Persistent (Geometry, Materials and Textures)
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Vertex
                                     { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Index
                                     { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }  // Materials
                                                                                                                                      // Textures
                                                                                                                                      // Samplers
                                 } )
                                 // Space 1: Scene Data
                                 .addSet( {
                                     { 0, Graphics::RHI::DescriptorType::UniformBuffer, Graphics::RHI::ShaderStage::All, 1 },         // Frame
                                     { 0, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Meshes
                                     { 1, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Materials
                                     { 2, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Instances
                                     { 3, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }, // Lights
                                     { 4, Graphics::RHI::DescriptorType::ReadonlyStorageBuffer, Graphics::RHI::ShaderStage::All, 1 }  // HW. Instance Redirection Buffer
                                 } )
                                 // Space 2: Instance ID Push Constant
                                 .setPushConstants( sizeof( uint ), 0, 2 )
                                 .enableIndirectRendering()
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

    Assets::GlobalMaterialRegistry::enumerate(
        [&]( const std::string& name, Assets::GlobalMaterialRegistry::SetupCallback setupFunc ) {
            MaterialArchetypeDesc desc;

            setupFunc( desc );

            _mtlLib.registerArchetype( desc );
        } );

    // _mtlLib.beginMaterial( "TestMaterial" )
    //     .addPass( MaterialPassType::Opaque,
    //               AXION_SHADER_DIR "/Slang/Materials/Test.slang",
    //               { { "vsForward", Graphics::ShaderType::Vertex },
    //                 { "psForward", Graphics::ShaderType::Pixel } } )
    //     .finish();
}

void Rasterizer::registerPasses() {
    _passes.registerPass<UploadPass>();
    _passes.registerPass<ForwardPass>();
    _passes.registerPass<ToneMappingPass>();
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

    // ----------------------- B. PER-FRAME BUFFERS (Volatile) -------------------
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

        _res.frame[i].indirectBufferHandle = r.buffer( "IndirectArgBuffer" + std::to_string( i ) )
                                                 .size( _settings.memory.volatileBufferSize )
                                                 .onCPU()
                                                 .asIndirect()
                                                 .create();

        auto* indBuffer = r.getBuffer( _res.frame[i].indirectBufferHandle );
        indBuffer->map();
        _res.frame[i].indirectAllocator = Graphics::RHI::LinearAllocator( indBuffer );
    }
}

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
    // 6. INSTANCE REDIRECTION DATA (for true instancing) (SSBO - Structured Buffer)
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

IndirectCommandData Rasterizer::uploadIndirectCommands(
    Graphics::RHI::LinearAllocator& indirectAlloc ) {
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

    auto cmdAlloc = indirectAlloc.allocate<Graphics::RHI::DrawIndexedIndirectCommand>( maxPossibleCommands );

    if ( !cmdAlloc.isValid() )
        return indirectData;

    auto*      cmdPtr       = (Graphics::RHI::DrawIndexedIndirectCommand*)cmdAlloc.cpuAddress;
    const auto RHI_CMD_SIZE = sizeof( Graphics::RHI::DrawIndexedIndirectCommand );

    indirectData.batches.reserve( _mtlLib.getArchetypesCount() );

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
            cmd.instanceCount = instanceAccumulator;
            cmd.firstIndex    = lastMesh.indexOffset / 4;
            cmd.vertexOffset  = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
            cmd.firstInstance = 0; // Unused

            cmd.baseInstanceID = batchStartOffsetInRedirect;

            cmdPtr[cmdWriteIdx++] = cmd;
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

        instanceAccumulator++;
    }

    // 3. Close
    // =================================================================================

    if ( instanceAccumulator > 0 )
    {
        Graphics::RHI::DrawIndexedIndirectCommand cmd;
        cmd.indexCount       = lastMesh.indexCount;
        cmd.instanceCount    = instanceAccumulator;
        cmd.firstIndex       = lastMesh.indexOffset / 4;
        cmd.vertexOffset     = (int)( lastMesh.vertexOffset / sizeof( Assets::Vertex ) );
        cmd.firstInstance    = 0;
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

    indirectData.bufferView      = cmdAlloc;
    indirectData.bufferView.size = cmdWriteIdx * RHI_CMD_SIZE;

    return indirectData;
}
} // namespace Core::Render

AXION_NAMESPACE_END