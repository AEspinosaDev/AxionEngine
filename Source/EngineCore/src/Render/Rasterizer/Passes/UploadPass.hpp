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
    struct Config {
        Rasterizer::Resources::GlobalBufferHandles outGlobalBufferHandles;

        STLW::Vector<Rasterizer::Resources::MeshUpload> meshUploads; ///< From reserveMeshes() (this frame)
        Graphics::BufferGPUFreeListAllocator* matAllocator = nullptr;

        Vector<Graphics::TextureHandle>* mtlTextureHandles = nullptr; ///< Owner handles of material textures, indexed by GPUScene texture slot
        GPUScene*                        gpuScene          = nullptr;
        u64                              maxAllocationSize = 0;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        // Early Exit (mesh queues are already drained by reserveMeshes)
        if ( !seedData.gpuScene ||
             ( seedData.meshUploads.empty() && !seedData.gpuScene->hasPendingUploads() && !seedData.gpuScene->hasPendingReleases() ) )
            return;

        builder.addPass<Config>( "UploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) { data.outGlobalBufferHandles.write( pb ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        u32 totalUsedSpace = 0;

        uploadMeshes( data, ctx, totalUsedSpace );
        processMaterials( data, ctx, totalUsedSpace );
        processTextures( data, ctx, totalUsedSpace );
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
