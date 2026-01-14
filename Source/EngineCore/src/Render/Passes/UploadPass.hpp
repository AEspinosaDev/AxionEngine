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
        GPUScene*           gpuScene             = nullptr;
        ulong               maxAllocationSize = 0;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        // Early Exit
        if ( !seedData.gpuScene || !seedData.gpuScene->hasPendingUploads() )
            return;

        builder.addPass<Config>( "UploadPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
                data.bufferHandles.vertex = pb.write( data.bufferHandles.vertex, Graphics::RHI::ResourceState::CopyDest );
                data.bufferHandles.index  = pb.write( data.bufferHandles.index,  Graphics::RHI::ResourceState::CopyDest ); },

                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        // Setup
        auto* cmd       = ctx.cmd;
        auto& scene     = *data.gpuScene;
        auto* allocator = ctx.transAllocator;

        uint totalUsedSpace = 0;

        //--------------------------------------------
        // GEOMETRY UPLOAD
        //--------------------------------------------

        auto* vb = ctx.getBuffer( data.bufferHandles.vertex );
        auto* ib = ctx.getBuffer( data.bufferHandles.index );

        static uint currentVtxOffset = 0;
        static uint currentIdxOffset = 0;

        auto& queue = scene.pendingMeshUploads();
        while ( !queue.empty() )
        {
            if ( totalUsedSpace >= (uint)data.maxAllocationSize )
                break;

            auto uploadEntry = std::move( queue.front() );
            queue.pop();

            auto& gpuMesh = scene.meshes()[uploadEntry.GPUMeshID];

            // Upload Vertices
            uint vtxSize = uploadEntry.vertices.size() * sizeof( Assets::Vertex );
            if ( vtxSize > 0 )
            {
                cmd->uploadBuffer( vb, uploadEntry.vertices.data(), vtxSize, currentVtxOffset, allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.vertexOffset = currentVtxOffset;
                currentVtxOffset += vtxSize;
            }

            // Upload Indices
            uint idxSize = uploadEntry.indices.size() * sizeof( uint );
            if ( idxSize > 0 )
            {
                cmd->uploadBuffer( ib, uploadEntry.indices.data(), idxSize, currentIdxOffset, allocator, Graphics::RHI::BarrierPolicy::None );
                gpuMesh.indexOffset = (uint)currentIdxOffset;
                currentIdxOffset += idxSize;
            }

            gpuMesh.valid = true;
            totalUsedSpace += vtxSize + idxSize;
        }

        //--------------------------------------------
        // MATERIAL UPLOAD
        //--------------------------------------------

        // TBD ...

        //--------------------------------------------
        // IMAGE UPLOAD
        //--------------------------------------------

        // TBD ...
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END