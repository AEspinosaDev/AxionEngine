#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/GPUScene.h>
#include <Render/PassManager.h>
#include <Render/Passes/GlobalBufferHandles.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class UploadPass : public IRenderPass
{
public:
    struct Config {
        GlobalBufferHandles outGlobalBufferHandles;

        Graphics::BufferGPUFreeListAllocator* vertexAllocator           = nullptr;
        Graphics::BufferGPUFreeListAllocator* indexAllocator            = nullptr;
        Graphics::BufferGPUFreeListAllocator* meshletAllocator          = nullptr;
        Graphics::BufferGPUFreeListAllocator* meshletVertexAllocator    = nullptr;
        Graphics::BufferGPUFreeListAllocator* meshletPrimitiveAllocator = nullptr;
        Graphics::BufferGPUFreeListAllocator* matAllocator              = nullptr;

        Vector<Graphics::TextureHandle>* mtlTextureHandles = nullptr; ///< Owner handles of material textures, indexed by GPUScene texture slot
        GPUScene*                        gpuScene          = nullptr;
        u64                              maxAllocationSize = 0;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        // Early Exit
        if ( !seedData.gpuScene ||
             ( !seedData.gpuScene->hasPendingUploads() && !seedData.gpuScene->hasPendingReleases() ) )
            return;

        builder.addPass<Config>( "UploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.outGlobalBufferHandles.write( pb ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    static constexpr u64 RAW_BUFFER_ALIGNMENT = 4; ///< ByteAddressBuffer loads need 4 B aligned addresses

    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        u32 totalUsedSpace = 0;

        processMeshes( data, ctx, totalUsedSpace );
        processMaterials( data, ctx, totalUsedSpace );
        processTextures( data, ctx, totalUsedSpace );
    }

    void processMeshes( const Config& data, Graphics::RenderPassContext& ctx, u32& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;

        auto* vb = ctx.getBuffer( data.outGlobalBufferHandles.vertex );
        auto* ib  = ctx.getBuffer( data.outGlobalBufferHandles.index );
        auto* mb  = ctx.getBuffer( data.outGlobalBufferHandles.meshlets );
        auto* mvb = ctx.getBuffer( data.outGlobalBufferHandles.meshletVertices );
        auto* mpb = ctx.getBuffer( data.outGlobalBufferHandles.meshletPrimitives );

        // 1. UPLOAD QUEUE
        auto& uploadQueue = scene.pendingMeshUploads();
        while ( !uploadQueue.empty() )
        {
            const auto& nextUpload           = uploadQueue.front();
            const auto* nextMeshlets         = nextUpload.geometryData->meshlets.get();
            u32         requiredVerticesSize = (u32)( nextUpload.geometryData->vertices.size() * sizeof( Assets::Vertex ) );
            u32         requiredIndicesSize  = (u32)( nextUpload.geometryData->indices.size() * sizeof( u32 ) );
            u32         requiredMeshletsSize = nextMeshlets ? (u32)( nextMeshlets->meshlets.size() * sizeof( Assets::Meshlet ) +
                                                                    nextMeshlets->vertexIndices.size() * sizeof( u32 ) +
                                                                    nextMeshlets->primitiveIndices.size() )
                                                          : 0;
            u32         totalRequiredSpace   = requiredVerticesSize + requiredIndicesSize + requiredMeshletsSize;

            if ( totalUsedSpace + totalRequiredSpace > (u32)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuMesh = scene.meshes()[uploadEntry.GPUMeshID];

            // A. Vertices
            auto vertexBufferSlice = data.vertexAllocator->allocate<Assets::Vertex>( uploadEntry.geometryData->vertices.size() );
            if ( vertexBufferSlice.size > 0 )
            {
                cmd->uploadBuffer( vb, uploadEntry.geometryData->vertices.data(), vertexBufferSlice.size, vertexBufferSlice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.vertexOffset = (u32)vertexBufferSlice.offset;
            }

            // B. Indices
            auto indexBufferSlice = data.indexAllocator->allocate<u32>( uploadEntry.geometryData->indices.size() );
            if ( indexBufferSlice.size > 0 )
            {
                cmd->uploadBuffer( ib, uploadEntry.geometryData->indices.data(), indexBufferSlice.size, indexBufferSlice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.indexOffset = (u32)indexBufferSlice.offset;
            }

            // C. Meshlets (only meshes cooked as meshlets)
            if ( const auto* meshlets = uploadEntry.geometryData->meshlets.get() )
            {
                auto meshletSlice = data.meshletAllocator->allocate<Assets::Meshlet>( meshlets->meshlets.size() );
                if ( meshletSlice.size > 0 )
                {
                    cmd->uploadBuffer( mb, meshlets->meshlets.data(), meshletSlice.size, meshletSlice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
                    gpuMesh.meshletOffset = (u32)meshletSlice.offset;
                }

                auto meshletVertexSlice = data.meshletVertexAllocator->allocate<u32>( meshlets->vertexIndices.size() );
                if ( meshletVertexSlice.size > 0 )
                {
                    cmd->uploadBuffer( mvb, meshlets->vertexIndices.data(), meshletVertexSlice.size, meshletVertexSlice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
                    gpuMesh.meshletVertexOffset = (u32)meshletVertexSlice.offset;
                }

                auto meshletPrimitiveSlice = data.meshletPrimitiveAllocator->allocate( meshlets->primitiveIndices.size(), RAW_BUFFER_ALIGNMENT );
                if ( meshletPrimitiveSlice.size > 0 )
                {
                    cmd->uploadBuffer( mpb, meshlets->primitiveIndices.data(), meshletPrimitiveSlice.size, meshletPrimitiveSlice.offset, *allocator, Graphics::RHI::BarrierPolicy::None );
                    gpuMesh.meshletPrimitiveOffset = (u32)meshletPrimitiveSlice.offset;
                }
            }

            gpuMesh.valid = true;
            totalUsedSpace += totalRequiredSpace;
        }

        // 2. DELETION QUEUE
        auto& deletionQueue = scene.pendingMeshReleases();
        while ( !deletionQueue.empty() )
        {
            auto deletionEntry = std::move( deletionQueue.front() );
            deletionQueue.pop();

            releaseSlice( data.vertexAllocator, vb, deletionEntry.vertexOffset, deletionEntry.vertexSize );
            releaseSlice( data.indexAllocator, ib, deletionEntry.indexOffset, deletionEntry.indexSize );
            releaseSlice( data.meshletAllocator, mb, deletionEntry.meshletOffset, deletionEntry.meshletSize );
            releaseSlice( data.meshletVertexAllocator, mvb, deletionEntry.meshletVertexOffset, deletionEntry.meshletVertexSize );
            releaseSlice( data.meshletPrimitiveAllocator, mpb, deletionEntry.meshletPrimitiveOffset, deletionEntry.meshletPrimitiveSize );
        }
    }

    static void releaseSlice( Graphics::BufferGPUFreeListAllocator* allocator, Graphics::RHI::IBuffer* container, u32 offset, u32 size ) {
        if ( size == 0 )
            return;

        Graphics::BufferSlice slice;
        slice.container = container;
        slice.offset    = offset;
        slice.size      = size;
        allocator->free( slice );
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