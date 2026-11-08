#include <Render/GPUScene.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer::Resources {

#pragma region Persistent

void createPersistentResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, GPUResources& res ) {

    //------------------------- A. MATERIAL BUFFER (Persistent) -------------------
    res.mtlBufferHandle = r.buffer( "GlobalMaterialBuffer" )
                              .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                              .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                              .size( budget.device.maxMaterialAlloc )
                              .onGPU()
                              .asRaw()
                              .create();
    res.mtlAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( res.mtlBufferHandle ) );

    //------------------------- B. FALLBACK RESOURCES -------------------
    STLW::Array<byte, 4> fallbackPixels = { 255, 0, 255, 255 };
    res.fallbackTexture2DHandle         = r.texture( "FallbackTexture2D" )
                                              .format( Graphics::Format::RGBA8_UNORM )
                                              .extent( { 1, 1, 1 } )
                                              .withData( fallbackPixels.data() )
                                              .create();

    res.fallbackSamplerHandle = r.sampler( "FallbackSampler" ).create();

    //------------------------- C. STATIC SAMPLERS -------------------
    res.linearClampSamplerHandle = r.sampler( "LinearClampSampler" )
                                       .address( Graphics::AddressMode::Clamp )
                                       .create();
}

#pragma endregion
#pragma region Transient

void createFrameResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, u32 framesInFlight, GPUResources& res ) {

    res.frame.resize( framesInFlight );
    for ( u32 i = 0; i < framesInFlight; ++i )
    {
        // ----------------------- A. PER-FRAME BUFFERS (Volatile) -------------------
        res.frame[i].uboBufferHandle = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                           .size( Config::MAX_GLOBAL_UBO_BYTES )
                                           .onCPU()
                                           .create();
        auto* uboBuffer              = r.getBuffer( res.frame[i].uboBufferHandle );
        uboBuffer->map();
        res.frame[i].uboAllocator = Graphics::BufferLinearAllocator<>( uboBuffer );

        res.frame[i].ssboBufferHandle = r.buffer( "GlobalSSBO_" + std::to_string( i ) )
                                            .size( budget.shared.maxConstantAllocPerFrame )
                                            .onCPU()
                                            .create();

        auto* ssboBuffer = r.getBuffer( res.frame[i].ssboBufferHandle );
        ssboBuffer->map();
        res.frame[i].ssboAllocator = Graphics::BufferLinearAllocator<>( ssboBuffer );

        // ----------------------- B. PER-FRAME INDIRECT RENDERING BUFFERS (Volatile) -------------------
        res.frame[i].indirectStagingBufferHandle = r.buffer( "IndirectStagingCommandBuffer_" + std::to_string( i ) )
                                                       .size( budget.shared.maxExecutableAlloc )
                                                       .onCPU()
                                                       .create();
        auto* indBuffer                          = r.getBuffer( res.frame[i].indirectStagingBufferHandle );
        indBuffer->map();
        res.frame[i].indirectAllocator = Graphics::BufferLinearAllocator<>( indBuffer );

        res.frame[i].indirectBufferHandle = r.buffer( "IndirectCommandBuffer_" + std::to_string( i ) )
                                                .size( budget.shared.maxExecutableAlloc )
                                                .asSSBO()
                                                .stride( sizeof( Graphics::RHI::DispatchMeshIndirectCommand ) )
                                                .create();

        res.frame[i].culledInstanceBufferHandle = r.buffer( "IndirectCulledInstanceBuffer_" + std::to_string( i ) )
                                                      .size( budget.shared.maxConstantAllocPerFrame )
                                                      .stride( sizeof( u32 ) )
                                                      .onGPU()
                                                      .asSSBO()
                                                      .view( Graphics::BufferViewFlags::BufferViewUnorderedAccess | Graphics::BufferViewFlags::BufferViewShaderResource ) // Written by culling, read by the vis pass
                                                      .create();

        // ----------------------- C. PER-FRAME PARTICLE SYSTEMS BUFFERS -------------------
        res.frame[i].particleVisibleCountBufferHandle = r.buffer( "ParticleVisibleCountBuffer_" + std::to_string( i ) )
                                                            .size( MAX_GPU_PARTICLE_SYSTEMS * sizeof( u32 ) )
                                                            .stride( sizeof( u32 ) )
                                                            .onGPU()
                                                            .asSSBO()
                                                            .view( Graphics::BufferViewFlags::BufferViewUnorderedAccess | Graphics::BufferViewFlags::BufferViewShaderResource )
                                                            .create();
    }
}

#pragma endregion
#pragma region Geometry

static constexpr u64 GEOMETRY_BUFFER_ALIGNMENT = 256; ///< Raw views need sizes multiple of 4, never 0 B buffers

GeometryPageSizes GeometryPageSizes::fromBudget( u64 pageSize ) {
    auto split = [pageSize]( u64 pct ) { return ( pageSize * pct ) / 100; };

    return { .vertices          = split( Config::GEOMETRY_BUDGET_VERTICES_PCT ),
             .indices           = split( Config::GEOMETRY_BUDGET_INDICES_PCT ),
             .meshlets          = split( Config::GEOMETRY_BUDGET_MESHLETS_PCT ),
             .meshletVertices   = split( Config::GEOMETRY_BUDGET_MESHLET_VERTICES_PCT ),
             .meshletPrimitives = split( Config::GEOMETRY_BUDGET_MESHLET_PRIMITIVES_PCT ) };
}

u32 createGeometryPage( Graphics::IGPUResourcePool& r, const GeometryPageSizes& sizes, GPUResources& res ) {

    const u32  pageID     = (u32)res.geometryPages.size();
    const auto prefix     = "GeometryPage" + std::to_string( pageID );
    auto       bufferSize = []( u64 size ) { return Helpers::alignUp<u64>( std::max<u64>( size, 1 ), GEOMETRY_BUFFER_ALIGNMENT ); };

    GeometryBuffersPage page;
    page.sizes = sizes;

    page.vertexBufferHandle = r.buffer( prefix + "_VertexBuffer" )
                                  .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                  .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                  .size( bufferSize( sizes.vertices ) )
                                  .onGPU()
                                  .asRaw()
                                  .create();
    page.vertexAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.vertexBufferHandle ) );

    page.indexBufferHandle = r.buffer( prefix + "_IndexBuffer" )
                                 .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                 .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                 .size( bufferSize( sizes.indices ) )
                                 .onGPU()
                                 .asRaw()
                                 .asIBO()
                                 .create();
    page.indexAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.indexBufferHandle ) );

    page.meshletBufferHandle = r.buffer( prefix + "_MeshletBuffer" )
                                   .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                   .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                   .size( bufferSize( sizes.meshlets ) )
                                   .onGPU()
                                   .asRaw()
                                   .create();
    page.meshletAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletBufferHandle ) );

    page.meshletVertexBufferHandle = r.buffer( prefix + "_MeshletVertexBuffer" )
                                         .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                         .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                         .size( bufferSize( sizes.meshletVertices ) )
                                         .onGPU()
                                         .asRaw()
                                         .create();
    page.meshletVertexAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletVertexBufferHandle ) );

    page.meshletPrimitiveBufferHandle = r.buffer( prefix + "_MeshletPrimitiveBuffer" )
                                            .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                            .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                            .size( bufferSize( sizes.meshletPrimitives ) )
                                            .onGPU()
                                            .asRaw()
                                            .create();
    page.meshletPrimitiveAllocator    = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletPrimitiveBufferHandle ) );

    res.geometryPages.pushBack( std::move( page ) );

    AXION_LOG_INFO( Logger::Module::Core, "Geometry page {} created ({} MB)", pageID, sizes.total() / ( 1024 * 1024 ) );
    return pageID;
}

namespace {

// Allocation alignments (same for the page search and the allocation)
constexpr u64 VERTEX_ALIGNMENT  = sizeof( Assets::Vertex );
constexpr u64 INDEX_ALIGNMENT   = sizeof( u32 );
constexpr u64 MESHLET_ALIGNMENT = sizeof( Assets::Meshlet );
constexpr u64 RAW_ALIGNMENT     = 4; ///< ByteAddressBuffer loads need 4 B aligned addresses

GeometryPageSizes requiredSizes( const Assets::GeometryData& geometry ) {
    const auto* meshlets = geometry.meshlets.get();

    return { .vertices          = geometry.vertices.size() * sizeof( Assets::Vertex ),
             .indices           = geometry.indices.size() * sizeof( u32 ),
             .meshlets          = meshlets ? meshlets->meshlets.size() * sizeof( Assets::Meshlet ) : 0,
             .meshletVertices   = meshlets ? meshlets->vertexIndices.size() * sizeof( u32 ) : 0,
             .meshletPrimitives = meshlets ? meshlets->primitiveIndices.size() : 0 };
}

/// @brief First page where every buffer of the mesh fits. INVALID if none
u32 findPage( GPUResources& res, const GeometryPageSizes& required ) {
    for ( u32 i = 0; i < (u32)res.geometryPages.size(); ++i )
    {
        auto& page = res.geometryPages[i];
        if ( page.vertexAllocator.canAllocate( required.vertices, VERTEX_ALIGNMENT ) &&
             page.indexAllocator.canAllocate( required.indices, INDEX_ALIGNMENT ) &&
             page.meshletAllocator.canAllocate( required.meshlets, MESHLET_ALIGNMENT ) &&
             page.meshletVertexAllocator.canAllocate( required.meshletVertices, INDEX_ALIGNMENT ) &&
             page.meshletPrimitiveAllocator.canAllocate( required.meshletPrimitives, RAW_ALIGNMENT ) )
            return i;
    }
    return AXION_INVALID_U32;
}

MeshUpload placeMesh( PendingMeshUpload&          uploadEntry,
                      u32                         pageID,
                      const GeometryPageSizes&    required,
                      GPUResources&               res,
                      Graphics::IGPUResourcePool& r,
                      GPUMesh&                    gpuMesh ) {
    auto& page = res.geometryPages[pageID];

    MeshUpload upload;
    upload.geometryData          = std::move( uploadEntry.geometryData );
    upload.vertexSlice           = page.vertexAllocator.allocate( required.vertices, VERTEX_ALIGNMENT );
    upload.indexSlice            = page.indexAllocator.allocate( required.indices, INDEX_ALIGNMENT );
    upload.meshletSlice          = page.meshletAllocator.allocate( required.meshlets, MESHLET_ALIGNMENT );
    upload.meshletVertexSlice    = page.meshletVertexAllocator.allocate( required.meshletVertices, INDEX_ALIGNMENT );
    upload.meshletPrimitiveSlice = page.meshletPrimitiveAllocator.allocate( required.meshletPrimitives, RAW_ALIGNMENT );

    // Residency (read by this frame GPUMesh upload)
    gpuMesh.geometryPageID         = pageID;
    gpuMesh.vertexHeapID           = r.getBuffer( page.vertexBufferHandle )->getBindlessSRVIdx();
    gpuMesh.indexHeapID            = r.getBuffer( page.indexBufferHandle )->getBindlessSRVIdx();
    gpuMesh.meshletHeapID          = r.getBuffer( page.meshletBufferHandle )->getBindlessSRVIdx();
    gpuMesh.meshletVertexHeapID    = r.getBuffer( page.meshletVertexBufferHandle )->getBindlessSRVIdx();
    gpuMesh.meshletPrimitiveHeapID = r.getBuffer( page.meshletPrimitiveBufferHandle )->getBindlessSRVIdx();

    gpuMesh.vertexOffset           = (u32)upload.vertexSlice.offset;
    gpuMesh.indexOffset            = (u32)upload.indexSlice.offset;
    gpuMesh.meshletOffset          = (u32)upload.meshletSlice.offset;
    gpuMesh.meshletVertexOffset    = (u32)upload.meshletVertexSlice.offset;
    gpuMesh.meshletPrimitiveOffset = (u32)upload.meshletPrimitiveSlice.offset;
    gpuMesh.valid                  = true;

    return upload;
}

void releaseSlice( Graphics::BufferGPUFreeListAllocator& allocator, Graphics::RHI::IBuffer* container, u32 offset, u32 size ) {
    if ( size == 0 )
        return;

    Graphics::BufferSlice slice;
    slice.container = container;
    slice.offset    = offset;
    slice.size      = size;
    allocator.free( slice );
}

void releaseMeshes( GPUScene& scene, GPUResources& res, Graphics::IGPUResourcePool& r ) {
    auto& deletionQueue = scene.pendingMeshReleases();
    while ( !deletionQueue.empty() )
    {
        auto deletionEntry = std::move( deletionQueue.front() );
        deletionQueue.pop();

        if ( deletionEntry.geometryPageID == AXION_INVALID_U32 ) // Never placed in a page
            continue;

        auto& page = res.geometryPages[deletionEntry.geometryPageID];
        releaseSlice( page.vertexAllocator, r.getBuffer( page.vertexBufferHandle ), deletionEntry.vertexOffset, deletionEntry.vertexSize );
        releaseSlice( page.indexAllocator, r.getBuffer( page.indexBufferHandle ), deletionEntry.indexOffset, deletionEntry.indexSize );
        releaseSlice( page.meshletAllocator, r.getBuffer( page.meshletBufferHandle ), deletionEntry.meshletOffset, deletionEntry.meshletSize );
        releaseSlice( page.meshletVertexAllocator, r.getBuffer( page.meshletVertexBufferHandle ), deletionEntry.meshletVertexOffset, deletionEntry.meshletVertexSize );
        releaseSlice( page.meshletPrimitiveAllocator, r.getBuffer( page.meshletPrimitiveBufferHandle ), deletionEntry.meshletPrimitiveOffset, deletionEntry.meshletPrimitiveSize );
    }
}

} // namespace

STLW::Vector<MeshUpload> reserveMeshes( GPUScene&                   scene,
                                        GPUResources&               res,
                                        Graphics::IGPUResourcePool& r,
                                        u64                         pageSize,
                                        u64                         maxAllocationSize ) {
    releaseMeshes( scene, res, r );

    STLW::Vector<MeshUpload> uploads;
    const auto               standardPage = GeometryPageSizes::fromBudget( pageSize );

    u64   usedSpace   = 0;
    auto& uploadQueue = scene.pendingMeshUploads();
    while ( !uploadQueue.empty() )
    {
        const auto required = requiredSizes( *uploadQueue.front().geometryData );

        // Staging is per frame: a mesh bigger than the whole budget would block the queue forever
        if ( required.total() > maxAllocationSize )
        {
            AXION_LOG_ERROR( Logger::Module::Core, "Mesh upload ({} MB) exceeds the per frame upload budget ({} MB): skipped", required.total() / ( 1024 * 1024 ), maxAllocationSize / ( 1024 * 1024 ) );
            uploadQueue.pop();
            continue;
        }

        if ( usedSpace + required.total() > maxAllocationSize )
            break; // Next frame

        auto uploadEntry = std::move( uploadQueue.front() );
        uploadQueue.pop();

        u32 pageID = findPage( res, required );
        if ( pageID == AXION_INVALID_U32 )
            pageID = createGeometryPage( r, standardPage.fits( required ) ? standardPage : required, res );

        uploads.push_back( placeMesh( uploadEntry, pageID, required, res, r, scene.meshes()[uploadEntry.GPUMeshID] ) );
        usedSpace += required.total();
    }

    return uploads;
}

#pragma endregion
#pragma region Particles

ParticleSystemBuffers createParticleSystemBuffers( Graphics::IGPUResourcePool& r, u32 slot, u32 particleCount ) {
    const auto createBuffer = [&]( const std::string& name, u32 stride ) {
        return r.buffer( name + "_" + std::to_string( slot ) )
            .size( (u64)particleCount * stride )
            .stride( stride )
            .onGPU()
            .asSSBO()
            .view( Graphics::BufferViewFlags::BufferViewUnorderedAccess | Graphics::BufferViewFlags::BufferViewShaderResource )
            .create();
    };

    return { .dataBufferHandle        = createBuffer( "ParticleDataBuffer", Config::PARTICLE_DATA_STRIDE ),
             .visibleListBufferHandle = createBuffer( "ParticleVisibleListBuffer", sizeof( u32 ) ) };
}

void reserveParticleBuffers( GPUScene& scene, GPUResources& res, Graphics::IGPUResourcePool& r ) {

    auto& buffers = scene.particleBuffers();
    res.particleBuffers.resize( buffers.size() );

    // Their lifetime is bound to the TTL, always bigger than the frames in flight: safe to destroy here
    auto& releaseQueue = scene.pendingParticleBufferReleases();
    while ( !releaseQueue.empty() )
    {
        const u32 slot = releaseQueue.front();
        releaseQueue.pop();

        r.destroyBuffer( res.particleBuffers[slot].dataBufferHandle );
        r.destroyBuffer( res.particleBuffers[slot].visibleListBufferHandle );
        res.particleBuffers[slot] = {};
    }

    auto& uploadQueue = scene.pendingParticleBufferUploads();
    while ( !uploadQueue.empty() )
    {
        const PendingParticleBufferUpload entry = uploadQueue.front();
        uploadQueue.pop();

        const auto systemBuffers = createParticleSystemBuffers( r, entry.slot, entry.particleCount );
        if ( !systemBuffers.dataBufferHandle.isValid() || !systemBuffers.visibleListBufferHandle.isValid() )
        {
            r.destroyBuffer( systemBuffers.dataBufferHandle );
            r.destroyBuffer( systemBuffers.visibleListBufferHandle );
            AXION_LOG_ERROR( Logger::Module::Core, "Failed to create the buffers of a particle system ({} particles)", entry.particleCount );
            AXION_LOG_FATAL_ERROR( Logger::Module::Core, "Failed to create the buffers of a particle system ({} particles)", entry.particleCount );
            continue;
        }
        res.particleBuffers[entry.slot] = systemBuffers;

        auto* dataBuffer        = r.getBuffer( systemBuffers.dataBufferHandle );
        auto* visibleListBuffer = r.getBuffer( systemBuffers.visibleListBufferHandle );

        auto& buffer                = buffers[entry.slot];
        buffer.srvHeapID            = dataBuffer->getBindlessSRVIdx();
        buffer.uavHeapID            = dataBuffer->getBindlessUAVIdx();
        buffer.visibleListSRVHeapID = visibleListBuffer->getBindlessSRVIdx();
        buffer.visibleListUAVHeapID = visibleListBuffer->getBindlessUAVIdx();
        buffer.needsReset           = true;
    }
}

#pragma endregion
#pragma region CSM

void reserveCSMs( GPUScene& scene, GPUResources& res, Graphics::IGPUResourcePool& r, Graphics::Format smFormat ) {
    auto& csms = scene.csms();
    res.csmTextureHandles.resize( csms.size() );

    // Their lifetime is bound to the TTL, always bigger than the frames in flight: safe to destroy here
    auto& releaseQueue = scene.pendingCSMReleases();
    while ( !releaseQueue.empty() )
    {
        const u32 slot = releaseQueue.front();
        releaseQueue.pop();

        r.destroyTexture( res.csmTextureHandles[slot] );
        res.csmTextureHandles[slot] = Graphics::TextureHandle(); // Invalid one
    }

    auto& uploadQueue = scene.pendingCSMUploads();
    while ( !uploadQueue.empty() )
    {
        const PendingCSMUpload entry = uploadQueue.front();
        uploadQueue.pop();

        // Same resolution the GPUScene snapped the cascades with (a new resolution gets a new slot)
        Graphics::TextureHandle csm = r.texture( "CSM_" + std::to_string( entry.slot ) )
                                          .format( smFormat )
                                          .flags( Graphics::TextureViewFlags::TextureViewShaderResource | Graphics::TextureViewFlags::TextureViewDepthStencil )
                                          .dim( Graphics::TextureDimension::Texture2DArray )
                                          .array( Config::CSM_MAX_SLICES )
                                          .extent( { entry.shadowRes, entry.shadowRes, 1 } )
                                          .create();

        if ( !csm.isValid() )
        {
            AXION_LOG_ERROR( Logger::Module::Core, "Failed to create a CSM ({}x{}, {} slices)", entry.shadowRes, entry.shadowRes, Config::CSM_MAX_SLICES );
            AXION_LOG_FATAL_ERROR( Logger::Module::Core, "Failed to create a CSM ({}x{}, {} slices)", entry.shadowRes, entry.shadowRes, Config::CSM_MAX_SLICES );
            continue;
        }
        res.csmTextureHandles[entry.slot] = csm;

        // Bindless: read through GPUShadow::mapSRVHeapID from the next GPUScene update (depth formats have no UAV)
        csms[entry.slot].srvHeapID = r.getTexture( csm )->getBindlessSRVIdx();
    }
}

#pragma endregion
#pragma region RenderGraph

GlobalBufferHandles importGlobalBuffers( Graphics::RenderGraphBuilder& builder, const GPUResources& res ) {

    GlobalBufferHandles handles;
    for ( u32 i = 0; i < (u32)res.geometryPages.size(); ++i )
    {
        const auto& page   = res.geometryPages[i];
        const auto  prefix = "GeometryPage" + std::to_string( i );

        handles.geometryPages.pushBack( { .vertex            = builder.import( prefix + "_VertexBuffer", page.vertexBufferHandle ),
                                          .index             = builder.import( prefix + "_IndexBuffer", page.indexBufferHandle ),
                                          .meshlets          = builder.import( prefix + "_MeshletBuffer", page.meshletBufferHandle ),
                                          .meshletVertices   = builder.import( prefix + "_MeshletVertexBuffer", page.meshletVertexBufferHandle ),
                                          .meshletPrimitives = builder.import( prefix + "_MeshletPrimitiveBuffer", page.meshletPrimitiveBufferHandle ) } );
    }
    handles.materials = builder.import( "GlobalMaterialBuffer", res.mtlBufferHandle );

    return handles;
}

#pragma endregion

} // namespace Core::Render::Rasterizer::Resources

AXION_NAMESPACE_END
