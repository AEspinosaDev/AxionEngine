#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/GPUScene.h>
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerResources.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class UploadPass : public IRenderPass
{
public:
    /// @brief A mesh already placed in a geometry page by reserveMeshes(), waiting for its copies
    struct MeshUpload {
        std::shared_ptr<Assets::GeometryData> geometryData;

        Graphics::BufferSlice vertexSlice;
        Graphics::BufferSlice indexSlice;
        Graphics::BufferSlice meshletSlice;
        Graphics::BufferSlice meshletVertexSlice;
        Graphics::BufferSlice meshletPrimitiveSlice;
    };

    struct Config {
        Rasterizer::Resources::GlobalBufferHandles outGlobalBufferHandles;

        STLW::Vector<MeshUpload>              meshUploads;            ///< From reserveMeshes() (this frame)
        Graphics::BufferGPUFreeListAllocator* matAllocator = nullptr;

        Vector<Graphics::TextureHandle>* mtlTextureHandles = nullptr; ///< Owner handles of material textures, indexed by GPUScene texture slot
        GPUScene*                        gpuScene          = nullptr;
        u64                              maxAllocationSize = 0;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    /// @brief CPU phase, before the frame data upload: GPUMesh gets its heap IDs & offsets the same frame its copies are recorded.
    /// Releases the GC'd meshes, then places the pending ones in the first page with room. When none has it, a new page is created:
    /// standard size, or dedicated (sized for the mesh) if the mesh doesn't fit a standard one
    STLW::Vector<MeshUpload> reserveMeshes( GPUScene&                            scene,
                                            Rasterizer::Resources::GPUResources& res,
                                            Graphics::IGPUResourcePool&          r,
                                            u64                                  pageSize,
                                            u64                                  maxAllocationSize ) {
        releaseMeshes( scene, res, r );

        STLW::Vector<MeshUpload> uploads;
        const auto               standardPage = Rasterizer::Resources::GeometryPageSizes::fromBudget( pageSize );

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
                pageID = Rasterizer::Resources::createGeometryPage( r, standardPage.fits( required ) ? standardPage : required, res );

            uploads.push_back( placeMesh( uploadEntry, pageID, required, res, r, scene.meshes()[uploadEntry.GPUMeshID] ) );
            usedSpace += required.total();
        }

        return uploads;
    }

    /// @brief CPU phase, like reserveMeshes: destroys the buffers the GC released and creates the pending ones.
    /// There are no copies: new buffers are initialized by the simulation, from the seed.
    void reserveParticleBuffers( GPUScene& scene, Rasterizer::Resources::GPUResources& res, Graphics::IGPUResourcePool& r ) {

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

            const auto systemBuffers = Rasterizer::Resources::createParticleSystemBuffers( r, entry.slot, entry.particleCount );
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

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        // Early Exit (mesh queues are already drained by reserveMeshes)
        if ( !seedData.gpuScene ||
             ( seedData.meshUploads.empty() && !seedData.gpuScene->hasPendingUploads() && !seedData.gpuScene->hasPendingReleases() ) )
            return;

        builder.addPass<Config>( "UploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.outGlobalBufferHandles.write( pb ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    // Allocation alignments (same for the page search and the allocation)
    static constexpr u64 VERTEX_ALIGNMENT  = sizeof( Assets::Vertex );
    static constexpr u64 INDEX_ALIGNMENT   = sizeof( u32 );
    static constexpr u64 MESHLET_ALIGNMENT = sizeof( Assets::Meshlet );
    static constexpr u64 RAW_ALIGNMENT     = 4; ///< ByteAddressBuffer loads need 4 B aligned addresses

    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        u32 totalUsedSpace = 0;

        uploadMeshes( data, ctx, totalUsedSpace );
        processMaterials( data, ctx, totalUsedSpace );
        processTextures( data, ctx, totalUsedSpace );
    }

    static Rasterizer::Resources::GeometryPageSizes requiredSizes( const Assets::GeometryData& geometry ) {
        const auto* meshlets = geometry.meshlets.get();

        return { .vertices          = geometry.vertices.size() * sizeof( Assets::Vertex ),
                 .indices           = geometry.indices.size() * sizeof( u32 ),
                 .meshlets          = meshlets ? meshlets->meshlets.size() * sizeof( Assets::Meshlet ) : 0,
                 .meshletVertices   = meshlets ? meshlets->vertexIndices.size() * sizeof( u32 ) : 0,
                 .meshletPrimitives = meshlets ? meshlets->primitiveIndices.size() : 0 };
    }

    /// @brief First page where every buffer of the mesh fits. INVALID if none
    static u32 findPage( Rasterizer::Resources::GPUResources& res, const Rasterizer::Resources::GeometryPageSizes& required ) {
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

    static MeshUpload placeMesh( PendingMeshUpload&                              uploadEntry,
                                 u32                                             pageID,
                                 const Rasterizer::Resources::GeometryPageSizes& required,
                                 Rasterizer::Resources::GPUResources&            res,
                                 Graphics::IGPUResourcePool&                     r,
                                 GPUMesh&                                        gpuMesh ) {
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

    static void releaseMeshes( GPUScene& scene, Rasterizer::Resources::GPUResources& res, Graphics::IGPUResourcePool& r ) {
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

    static void releaseSlice( Graphics::BufferGPUFreeListAllocator& allocator, Graphics::RHI::IBuffer* container, u32 offset, u32 size ) {
        if ( size == 0 )
            return;

        Graphics::BufferSlice slice;
        slice.container = container;
        slice.offset    = offset;
        slice.size      = size;
        allocator.free( slice );
    }

    void uploadMeshes( const Config& data, Graphics::RenderPassContext& ctx, u32& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto* allocator = ctx.transAllocator;

        auto upload = [&]( const Graphics::BufferSlice& slice, const void* src ) {
            if ( slice.size == 0 )
                return;
            cmd->uploadBuffer( slice.container, src, slice.size, slice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
            totalUsedSpace += (u32)slice.size;
        };

        for ( const auto& meshUpload : data.meshUploads )
        {
            const auto& geometry = *meshUpload.geometryData;

            upload( meshUpload.vertexSlice, geometry.vertices.data() );
            upload( meshUpload.indexSlice, geometry.indices.data() );

            // Meshlets (only meshes cooked as meshlets)
            if ( const auto* meshlets = geometry.meshlets.get() )
            {
                upload( meshUpload.meshletSlice, meshlets->meshlets.data() );
                upload( meshUpload.meshletVertexSlice, meshlets->vertexIndices.data() );
                upload( meshUpload.meshletPrimitiveSlice, meshlets->primitiveIndices.data() );
            }
        }
    }

    void processMaterials( const Config& data, Graphics::RenderPassContext& ctx, u32& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;
        auto* mtlb      = ctx.getBuffer( data.outGlobalBufferHandles.materials );

        const u64 ALIGNMENT = 16;
        // 1. UPLOAD QUEUE
        auto& uploadQueue = scene.pendingMaterialUploads();
        while ( !uploadQueue.empty() )
        {
            if ( totalUsedSpace >= (u32)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuMtl = scene.materials()[uploadEntry.GPUMaterialID];

            // --- FIX MEMORY LEAK (UPDATE LOGIC) ---
            // Si el material ya existía en VRAM (es un update, no un create),
            // liberamos la memoria vieja antes de pedir nueva.
            // NOTA: Podrías intentar reutilizarla si el tamaño es igual,
            // pero Free+Alloc evita fragmentación si el tamaño cambia.
            if ( gpuMtl.bufferOffset != AXION_INVALID_U32 ) // Already in VRAM (offset 0 is a valid offset)
            {
                Graphics::BufferSlice oldSlice;
                oldSlice.container = mtlb;
                oldSlice.offset    = gpuMtl.bufferOffset;
                oldSlice.size      = gpuMtl.payloadSize; // Usamos el tamaño viejo guardado en GPU struct

                data.matAllocator->free( oldSlice );

                gpuMtl.bufferOffset = AXION_INVALID_U32;
            }

            auto mtlBufferView = data.matAllocator->allocate( uploadEntry.payload.size(), ALIGNMENT );

            if ( mtlBufferView.size > 0 )
            {
                cmd->uploadBuffer( mtlb,
                                   uploadEntry.payload.data(),
                                   mtlBufferView.size,
                                   mtlBufferView.offset,
                                   *allocator,
                                   Graphics::RHI::BarrierPolicy::None );

                gpuMtl.bufferOffset = (u32)mtlBufferView.offset;
                gpuMtl.payloadSize  = (u32)uploadEntry.payload.size();
            }

            totalUsedSpace += (u32)mtlBufferView.size;
        }

        // 2. DELETION QUEUE
        auto& deletionQueue = scene.pendingMaterialReleases();
        while ( !deletionQueue.empty() )
        {
            auto deletionEntry = std::move( deletionQueue.front() );
            deletionQueue.pop();

            if ( deletionEntry.payloadSize > 0 && deletionEntry.bufferOffset != AXION_INVALID_U32 ) // Never uploaded: nothing to free
            {
                Graphics::BufferSlice mView;
                mView.container = mtlb;
                mView.offset    = deletionEntry.bufferOffset;
                mView.size      = deletionEntry.payloadSize;

                data.matAllocator->free( mView );
            }
        }
    }

    void processTextures( const Config& data, Graphics::RenderPassContext& ctx, u32& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;

        // 1. Upload Queue
        auto& uploadQueue = scene.pendingTextureUploads();
        while ( !uploadQueue.empty() )
        {
            auto& nextUpload = uploadQueue.front();

            size_t pixelSize = 0;
            if ( nextUpload.pixels )
            {
                if ( std::holds_alternative<STLW::Vector<byte>>( *nextUpload.pixels ) )
                    pixelSize = std::get<STLW::Vector<byte>>( *nextUpload.pixels ).size() * sizeof( byte );
                else if ( std::holds_alternative<STLW::Vector<float>>( *nextUpload.pixels ) )
                    pixelSize = std::get<STLW::Vector<float>>( *nextUpload.pixels ).size() * sizeof( float );
            }

            u32 requiredSpace = (u32)pixelSize + AXION_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

            if ( totalUsedSpace + requiredSpace > (u32)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuTex = scene.textures()[uploadEntry.slot];

            if ( pixelSize > 0 )
            {
                const void* pixelData = nullptr;
                if ( std::holds_alternative<STLW::Vector<byte>>( *uploadEntry.pixels ) )
                    pixelData = std::get<STLW::Vector<byte>>( *uploadEntry.pixels ).data();
                else if ( std::holds_alternative<STLW::Vector<float>>( *uploadEntry.pixels ) )
                    pixelData = std::get<STLW::Vector<float>>( *uploadEntry.pixels ).data();

                if ( pixelData != nullptr )
                {
                    // Create texture
                    ( *data.mtlTextureHandles )[uploadEntry.slot] = ctx.resources.texture( uploadEntry.name )
                                                                        .extent( uploadEntry.extent )
                                                                        .format( uploadEntry.format )
                                                                        .create();

                    auto* tex = ctx.resources.getTexture( ( *data.mtlTextureHandles )[uploadEntry.slot] );

                    // Upload
                    cmd->barrier( tex, Graphics::RHI::ResourceState::CopyDest );
                    cmd->uploadTexture( tex, pixelData, *allocator, 0, 0, Graphics::RHI::BarrierPolicy::None );
                    cmd->barrier( tex, Graphics::RHI::ResourceState::ShaderResource );

                    // Bindless: shaders reach it through gTextures[slot].heapID (visible from next frame's gTextures upload)
                    gpuTex.heapID = tex->getBindlessSRVIdx();
                    totalUsedSpace += requiredSpace;
                }
            }
        }
        // 2. DELETION QUEUE
        auto& deletionQueue = scene.pendingTextureReleases();
        while ( !deletionQueue.empty() )
        {
            auto deletionEntry = std::move( deletionQueue.front() );
            deletionQueue.pop();

            // Textures are always save to destroy here as its lifetime is bound bt TTL, and is always bigger than the frame in flight number
            ctx.resources.destroyTexture( ( *data.mtlTextureHandles )[deletionEntry] );

            ( *data.mtlTextureHandles )[deletionEntry] = Graphics::TextureHandle();
        }
    }
};
} // namespace Core::Render
AXION_NAMESPACE_END
