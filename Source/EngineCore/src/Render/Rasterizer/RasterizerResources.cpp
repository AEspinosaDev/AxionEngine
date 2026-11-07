#include <Render/GPUScene.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer::Resources {

static constexpr u64 GEOMETRY_BUFFER_ALIGNMENT = 256; ///< Raw views need sizes multiple of 4, never 0 B buffers

GeometryPageSizes GeometryPageSizes::fromBudget( u64 pageSize ) {
    auto split = [pageSize]( u64 pct ) { return ( pageSize * pct ) / 100; };

    return { .vertices          = split( Config::GEOMETRY_BUDGET_VERTICES_PCT ),
             .indices           = split( Config::GEOMETRY_BUDGET_INDICES_PCT ),
             .meshlets          = split( Config::GEOMETRY_BUDGET_MESHLETS_PCT ),
             .meshletVertices   = split( Config::GEOMETRY_BUDGET_MESHLET_VERTICES_PCT ),
             .meshletPrimitives = split( Config::GEOMETRY_BUDGET_MESHLET_PRIMITIVES_PCT ) };
}

void createPersistentResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, GPUResources& res ) {

    //------------------------- A. MATERIAL BUFFER (Persistent) -------------------
    res.mtlBufferHandle = r.buffer( "GlobalMaterialBuffer" )
                              .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                              .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                              .size( budget.device.maxMaterialAlloc )
                              .onGPU()
                              .asRaw()
                              .create();
    res.mtlAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( res.mtlBufferHandle ) );

    //------------------------- B. FALLBACK RESOURCES -------------------
    STLW::Array<byte, 4> fallbackPixels = { 255, 0, 255, 255 };
    res.fallbackTexture2DHandle         = r.texture( "FallbackTexture2D" )
                                      .format( Graphics::Format::RGBA8_UNORM )
                                      .extent( { 1, 1, 1 } )
                                      .withData( fallbackPixels.data() )
                                      .create();

    res.fallbackSamplerHandle = r.sampler( "FallbackSampler" ).create();

    res.linearClampSamplerHandle = r.sampler( "LinearClampSampler" )
                                       .address( Graphics::AddressMode::Clamp )
                                       .create();
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
    page.vertexAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.vertexBufferHandle ) );

    page.indexBufferHandle = r.buffer( prefix + "_IndexBuffer" )
                                 .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                 .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                 .size( bufferSize( sizes.indices ) )
                                 .onGPU()
                                 .asRaw()
                                 .asIBO()
                                 .create();
    page.indexAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.indexBufferHandle ) );

    page.meshletBufferHandle = r.buffer( prefix + "_MeshletBuffer" )
                                   .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                   .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                   .size( bufferSize( sizes.meshlets ) )
                                   .onGPU()
                                   .asRaw()
                                   .create();
    page.meshletAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletBufferHandle ) );

    page.meshletVertexBufferHandle = r.buffer( prefix + "_MeshletVertexBuffer" )
                                         .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                         .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                         .size( bufferSize( sizes.meshletVertices ) )
                                         .onGPU()
                                         .asRaw()
                                         .create();
    page.meshletVertexAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletVertexBufferHandle ) );

    page.meshletPrimitiveBufferHandle = r.buffer( prefix + "_MeshletPrimitiveBuffer" )
                                            .usage( Graphics::BufferUsage::TransferDst | Graphics::BufferUsage::Storage )
                                            .view( Graphics::BufferViewFlags::BufferViewShaderResource )
                                            .size( bufferSize( sizes.meshletPrimitives ) )
                                            .onGPU()
                                            .asRaw()
                                            .create();
    page.meshletPrimitiveAllocator = Graphics::BufferGPUFreeListAllocator( r.getBuffer( page.meshletPrimitiveBufferHandle ) );

    res.geometryPages.pushBack( std::move( page ) );

    AXION_LOG_INFO( Logger::Module::Core, "Geometry page {} created ({} MB)", pageID, sizes.total() / ( 1024 * 1024 ) );
    return pageID;
}

void createFrameResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, u32 framesInFlight, GPUResources& res ) {

    res.frame.resize( framesInFlight );
    for ( u32 i = 0; i < framesInFlight; ++i )
    {
        // ----------------------- A. PER-FRAME BUFFERS (Volatile) -------------------
        res.frame[i].uboBufferHandle = r.buffer( "GlobalUBO_" + std::to_string( i ) )
                                           .size( Config::MAX_GLOBAL_UBO_BYTES )
                                           .onCPU()
                                           .create();
        auto* uboBuffer = r.getBuffer( res.frame[i].uboBufferHandle );
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
        auto* indBuffer = r.getBuffer( res.frame[i].indirectStagingBufferHandle );
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

} // namespace Core::Render::Rasterizer::Resources

AXION_NAMESPACE_END
