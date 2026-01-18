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
        GlobalBufferHandles bufferHandles;

        Graphics::RHI::FreeListAllocator* vertexAllocator = nullptr;
        Graphics::RHI::FreeListAllocator* indexAllocator  = nullptr;
        Graphics::RHI::FreeListAllocator* matAllocator    = nullptr;

        GPUScene* gpuScene          = nullptr;
        ulong     maxAllocationSize = 0;
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
                data.bufferHandles.vertex = pb.write( data.bufferHandles.vertex, Graphics::RHI::ResourceState::CopyDest );
                data.bufferHandles.index  = pb.write( data.bufferHandles.index,  Graphics::RHI::ResourceState::CopyDest ); },

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

        auto* vb = ctx.getBuffer( data.bufferHandles.vertex );
        auto* ib = ctx.getBuffer( data.bufferHandles.index );

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
        // TBD: Logic for Material Buffer Upload & GC
    }

    void processTextures( const Config& data, Graphics::RenderPassContext& ctx, uint& totalUsedSpace ) {
        // TBD: Logic for Texture Copy & GC
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END