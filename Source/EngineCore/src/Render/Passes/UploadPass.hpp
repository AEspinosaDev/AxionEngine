#pragma once
#include "../GPUScene.h"
#include "../PassSystem.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class UploadPass : public IRenderPass
{
public:
    struct GlobalBufferHandles {
        Graphics::RGResourceHandle vertex;
        Graphics::RGResourceHandle index;
        Graphics::RGResourceHandle materials;
    };
    struct Config {
        GlobalBufferHandles outGlobalBufferHandles;

        Graphics::RHI::FreeListAllocator* vertexAllocator = nullptr;
        Graphics::RHI::FreeListAllocator* indexAllocator  = nullptr;
        Graphics::RHI::FreeListAllocator* matAllocator    = nullptr;

        std::vector<Graphics::RHI::IDescriptorSet*> allPersistentSets;

        std::vector<Graphics::TextureHandle>* mtlTextureHandles = nullptr;
        GPUScene*                             gpuScene          = nullptr;
        ulong                                 maxAllocationSize = 0;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        // Early Exit
        if ( !seedData.gpuScene ||
             ( !seedData.gpuScene->hasPendingUploads() && !seedData.gpuScene->hasPendingReleases() ) )
            return;

        builder.addPass<Config>( "UploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.outGlobalBufferHandles.vertex = pb.write( data.outGlobalBufferHandles.vertex, Graphics::RHI::ResourceState::CopyDest );
                data.outGlobalBufferHandles.index = pb.write( data.outGlobalBufferHandles.index, Graphics::RHI::ResourceState::CopyDest );
                data.outGlobalBufferHandles.materials  = pb.write( data.outGlobalBufferHandles.materials,  Graphics::RHI::ResourceState::CopyDest ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        uint totalUsedSpace = 0;

        processMeshes( data, ctx, totalUsedSpace );
        processMaterials( data, ctx, totalUsedSpace );
        processTextures( data, ctx, totalUsedSpace );
    }

    void processMeshes( const Config& data, Graphics::RenderPassContext& ctx, uint& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;

        auto* vb = ctx.getBuffer( data.outGlobalBufferHandles.vertex );
        auto* ib = ctx.getBuffer( data.outGlobalBufferHandles.index );

        // 1. UPLOAD QUEUE
        auto& uploadQueue = scene.pendingMeshUploads();
        while ( !uploadQueue.empty() )
        {
            if ( totalUsedSpace >= (uint)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuMesh = scene.meshes()[uploadEntry.GPUMeshID];

            // A. Vertices
            auto vertexBufferView = data.vertexAllocator->allocate<Assets::Vertex>( uploadEntry.vertices.size() );
            if ( vertexBufferView.size > 0 )
            {
                cmd->uploadBuffer( vb, uploadEntry.vertices.data(), vertexBufferView.size, vertexBufferView.offset, allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.vertexOffset = (uint)vertexBufferView.offset;
            }

            // B. Indices
            auto indexBufferView = data.indexAllocator->allocate<uint>( uploadEntry.indices.size() );
            if ( indexBufferView.size > 0 )
            {
                cmd->uploadBuffer( ib, uploadEntry.indices.data(), indexBufferView.size, indexBufferView.offset, allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.indexOffset = (uint)indexBufferView.offset;
            }

            gpuMesh.valid = true;
            totalUsedSpace += indexBufferView.size + vertexBufferView.size;
        }

        // 2. DELETION QUEUE
        auto& deletionQueue = scene.pendingMeshReleases();
        while ( !deletionQueue.empty() )
        {
            auto deletionEntry = std::move( deletionQueue.front() );
            deletionQueue.pop();

            if ( deletionEntry.vertexSize > 0 )
            {
                Graphics::RHI::BufferView vView;
                vView.buffer = vb;
                vView.offset = deletionEntry.vertexOffset;
                vView.size   = deletionEntry.vertexSize;
                data.vertexAllocator->free( vView );
            }

            if ( deletionEntry.indexSize > 0 )
            {
                Graphics::RHI::BufferView iView;
                iView.buffer = ib;
                iView.offset = deletionEntry.indexOffset;
                iView.size   = deletionEntry.indexSize;
                data.indexAllocator->free( iView );
            }
        }
    }

    void processMaterials( const Config& data, Graphics::RenderPassContext& ctx, uint& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;
        auto* mtlb      = ctx.getBuffer( data.outGlobalBufferHandles.materials );

        const ulong ALIGNMENT = 16;
        // 1. UPLOAD QUEUE
        auto& uploadQueue = scene.pendingMaterialUploads();
        while ( !uploadQueue.empty() )
        {
            if ( totalUsedSpace >= (uint)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuMtl = scene.materials()[uploadEntry.GPUMaterialID];

            // --- FIX MEMORY LEAK (UPDATE LOGIC) ---
            // Si el material ya existía en VRAM (es un update, no un create),
            // liberamos la memoria vieja antes de pedir nueva.
            // NOTA: Podrías intentar reutilizarla si el tamaño es igual,
            // pero Free+Alloc evita fragmentación si el tamaño cambia.
            if ( gpuMtl.bufferOffset != 0 ) // Asumiendo que 0 es null/inválido
            {
                Graphics::RHI::BufferView oldView;
                oldView.buffer = mtlb;
                oldView.offset = gpuMtl.bufferOffset;
                oldView.size   = gpuMtl.payloadSize; // Usamos el tamaño viejo guardado en GPU struct

                data.matAllocator->free( oldView );

                gpuMtl.bufferOffset = 0;
            }

            auto mtlBufferView = data.matAllocator->allocate( uploadEntry.payload.size(), ALIGNMENT );

            if ( mtlBufferView.size > 0 )
            {
                cmd->uploadBuffer( mtlb,
                                   uploadEntry.payload.data(),
                                   mtlBufferView.size,
                                   mtlBufferView.offset,
                                   allocator,
                                   Graphics::RHI::BarrierPolicy::None );

                gpuMtl.bufferOffset = (uint)mtlBufferView.offset;
                gpuMtl.payloadSize  = (uint)uploadEntry.payload.size();
            }

            totalUsedSpace += (uint)mtlBufferView.size;
        }

        // 2. DELETION QUEUE
        auto& deletionQueue = scene.pendingMaterialReleases();
        while ( !deletionQueue.empty() )
        {
            auto deletionEntry = std::move( deletionQueue.front() );
            deletionQueue.pop();

            if ( deletionEntry.payloadSize > 0 )
            {
                Graphics::RHI::BufferView mView;
                mView.buffer = mtlb;
                mView.offset = deletionEntry.bufferOffset;
                mView.size   = deletionEntry.payloadSize;

                data.matAllocator->free( mView );
            }
        }
    }

    void processTextures( const Config& data, Graphics::RenderPassContext& ctx, uint& totalUsedSpace ) {

        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;

        auto& uploadQueue = scene.pendingTextureUploads();
        while ( !uploadQueue.empty() )
        {
            auto& nextUpload = uploadQueue.front();

            size_t pixelSize = 0;
            if ( nextUpload.pixels )
            {
                if ( std::holds_alternative<std::vector<uchar>>( *nextUpload.pixels ) )
                    pixelSize = std::get<std::vector<uchar>>( *nextUpload.pixels ).size() * sizeof( uchar );
                else if ( std::holds_alternative<std::vector<float>>( *nextUpload.pixels ) )
                    pixelSize = std::get<std::vector<float>>( *nextUpload.pixels ).size() * sizeof( float );
            }

            uint requiredSpace = (uint)pixelSize + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

            if ( totalUsedSpace + requiredSpace > (uint)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( uploadQueue.front() );
            uploadQueue.pop();

            auto& gpuTex = scene.textures()[uploadEntry.slot];

            if ( pixelSize > 0 )
            {
                const void* pixelData = nullptr;
                if ( std::holds_alternative<std::vector<uchar>>( *uploadEntry.pixels ) )
                    pixelData = std::get<std::vector<uchar>>( *uploadEntry.pixels ).data();
                else if ( std::holds_alternative<std::vector<float>>( *uploadEntry.pixels ) )
                    pixelData = std::get<std::vector<float>>( *uploadEntry.pixels ).data();

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
                    cmd->uploadTexture( tex, pixelData, allocator, 0, 0, Graphics::RHI::BarrierPolicy::None );
                    cmd->barrier( tex, Graphics::RHI::ResourceState::ShaderResource );

                    gpuTex.slot = uploadEntry.slot;
                    totalUsedSpace += requiredSpace;

                    // Update bindless slots
                    for ( auto* pSet : data.allPersistentSets )
                    {
                        pSet->attachBindless( 3, uploadEntry.slot, tex, Graphics::RHI::ResourceState::ShaderResource );
                    }
                }
            }
        }
    }
};
} // namespace Core::Render
AXION_NAMESPACE_END