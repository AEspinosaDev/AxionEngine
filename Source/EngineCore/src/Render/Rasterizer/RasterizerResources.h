#pragma once
#include <Axion/Common/Containers/STLWrapper/Vector.h>
#include <Axion/Common/Containers/SmallVector.h>
#include <Axion/Common/Containers/Vector.h>
#include <Axion/Core/Render/Common.h>
#include <Axion/Graphics/Subsystems/IRenderGraph.h>

#include <memory>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {
struct GeometryData;
}

namespace Core::Render {
class GPUScene;
}

namespace Core::Render::Rasterizer {
namespace Resources {

constexpr u32 INLINE_GEOMETRY_PAGES = 5; ///< Pages stored inline (more spill to the heap)

//-----------------------------------------------------------------------------
// Geometry Pool
//-----------------------------------------------------------------------------

/// @brief Bytes of each buffer of a geometry page (also used for the requirements of a single mesh)
struct GeometryPageSizes {
    u64 vertices          = 0;
    u64 indices           = 0;
    u64 meshlets          = 0;
    u64 meshletVertices   = 0;
    u64 meshletPrimitives = 0;

    /// @brief Standard page: pageSize split by the Config::GEOMETRY_BUDGET_*_PCT ratios
    static GeometryPageSizes fromBudget( u64 pageSize );

    /// @brief True if every buffer of `other` fits in the same buffer of this one
    bool fits( const GeometryPageSizes& other ) const {
        return other.vertices <= vertices && other.indices <= indices && other.meshlets <= meshlets &&
               other.meshletVertices <= meshletVertices && other.meshletPrimitives <= meshletPrimitives;
    }
    u64 total() const { return vertices + indices + meshlets + meshletVertices + meshletPrimitives; }
};

/// @brief One page of the geometry pool: a set of bindless buffers that sub-allocates whole meshes (a mesh never straddles pages).
/// Pages are created on demand by the UploadPass. A mesh that doesn't fit a standard page gets a dedicated one, sized for it
struct GeometryBuffersPage {
    Graphics::BufferHandle               vertexBufferHandle;
    Graphics::BufferGPUFreeListAllocator vertexAllocator;

    Graphics::BufferHandle               indexBufferHandle;
    Graphics::BufferGPUFreeListAllocator indexAllocator;

    Graphics::BufferHandle               meshletBufferHandle;
    Graphics::BufferGPUFreeListAllocator meshletAllocator;

    Graphics::BufferHandle               meshletVertexBufferHandle;
    Graphics::BufferGPUFreeListAllocator meshletVertexAllocator;

    Graphics::BufferHandle               meshletPrimitiveBufferHandle;
    Graphics::BufferGPUFreeListAllocator meshletPrimitiveAllocator;

    GeometryPageSizes sizes;
};

//-----------------------------------------------------------------------------
// Rasterizer Resources
//-----------------------------------------------------------------------------

struct FrameResources {
    Graphics::BufferHandle            uboBufferHandle;
    Graphics::BufferLinearAllocator<> uboAllocator;

    Graphics::BufferHandle            ssboBufferHandle;
    Graphics::BufferLinearAllocator<> ssboAllocator;

    // Indirect Rendering
    Graphics::BufferHandle            indirectStagingBufferHandle;
    Graphics::BufferLinearAllocator<> indirectAllocator;
    Graphics::BufferHandle            indirectBufferHandle;
    Graphics::BufferHandle            culledInstanceBufferHandle;

    // Particle systems
    Graphics::BufferHandle particleVisibleCountBufferHandle; ///< Visible particles per system (written by their update)
};

/// @brief GPU buffers of one particle system (bindless, heap IDs stored per GPUParticleSystem)
struct ParticleSystemBuffers {
    Graphics::BufferHandle dataBufferHandle;        ///< State of every particle. Written by the simulation, read by the drawing
    Graphics::BufferHandle visibleListBufferHandle; ///< Indices of the visible particles. Written by the culling, read by the drawing
};

struct GPUResources {
    // Geometry pool (bindless, heap IDs stored per GPUMesh)
    SmallVector<GeometryBuffersPage, INLINE_GEOMETRY_PAGES> geometryPages;

    // Materials (bindless, heap ID in GPUFrame)
    Graphics::BufferHandle               mtlBufferHandle;
    Graphics::BufferGPUFreeListAllocator mtlAllocator;

    Vector<Graphics::TextureHandle> materialTextureHandles; ///< Indexed by GPUScene texture slot
    Graphics::TextureHandle         fallbackTexture2DHandle;
    Graphics::SamplerHandle         fallbackSamplerHandle;
    Graphics::SamplerHandle         linearClampSamplerHandle; ///< Screen space resampling (TAA history): no wrapping at the borders

    // Particle systems
    Vector<ParticleSystemBuffers> particleBuffers; ///< Indexed by GPUScene particle buffer slot

    // Cascaded Shadow Maps
    SmallVector<Graphics::TextureHandle, 3> csmTextureHandles; ///< Indexed by GPUScene CSM slot

    SmallVector<FrameResources, 3> frame;
};

/// @brief Material buffer + utility resources (fallback texture & sampler)
void createPersistentResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, GPUResources& res );
/// @brief Creates a new geometry page with the given buffer sizes. Returns its page ID
u32 createGeometryPage( Graphics::IGPUResourcePool& r, const GeometryPageSizes& sizes, GPUResources& res );
/// @brief Per frame in flight buffers (UBO / SSBO rings and indirect rendering)
void createFrameResources( Graphics::IGPUResourcePool& r, const MemoryBudget& budget, u32 framesInFlight, GPUResources& res );

/// @brief Creates the bindless buffers of a particle system. A handle is invalid if its creation failed
ParticleSystemBuffers createParticleSystemBuffers( Graphics::IGPUResourcePool& r, u32 slot, u32 particleCount );

//-----------------------------------------------------------------------------
// Residency (CPU phase, before the frame data upload: the GPUScene caches get their heap IDs before being uploaded)
//-----------------------------------------------------------------------------

/// @brief A mesh already placed in a geometry page by reserveMeshes(), waiting for its copies (UploadPass)
struct MeshUpload {
    std::shared_ptr<Assets::GeometryData> geometryData;

    Graphics::BufferSlice vertexSlice;
    Graphics::BufferSlice indexSlice;
    Graphics::BufferSlice meshletSlice;
    Graphics::BufferSlice meshletVertexSlice;
    Graphics::BufferSlice meshletPrimitiveSlice;
};

/// @brief GPUMesh gets its heap IDs & offsets the same frame its copies are recorded.
/// Releases the GC'd meshes, then places the pending ones in the first page with room. When none has it, a new page is created:
/// standard size, or dedicated (sized for the mesh) if the mesh doesn't fit a standard one
STLW::Vector<MeshUpload> reserveMeshes( GPUScene&                   scene,
                                        GPUResources&               res,
                                        Graphics::IGPUResourcePool& r,
                                        u64                         pageSize,
                                        u64                         maxAllocationSize );

/// @brief Like reserveMeshes: destroys the buffers the GC released and creates the pending ones.
/// There are no copies: new buffers are initialized by the simulation, from the seed.
void reserveParticleBuffers( GPUScene& scene, GPUResources& res, Graphics::IGPUResourcePool& r );

/// @brief Like reserveParticleBuffers, for the CSM texture arrays
void reserveCSMs( GPUScene& scene, GPUResources& res, Graphics::IGPUResourcePool& r, Graphics::Format smFormat );

//-----------------------------------------------------------------------------
// Render Graph Handles
//-----------------------------------------------------------------------------

/// @brief Render graph handles of one geometry page
struct GeometryPageHandles {
    Graphics::RGResourceHandle vertex;
    Graphics::RGResourceHandle index;
    Graphics::RGResourceHandle meshlets;
    Graphics::RGResourceHandle meshletVertices;
    Graphics::RGResourceHandle meshletPrimitives;
};

/// @brief Render graph handles of the persistent buffers (geometry pages + materials).
/// Bindless reads are invisible to the graph: passes reading geometry or materials declare them all to get the state transitions
struct GlobalBufferHandles {
    SmallVector<GeometryPageHandles, INLINE_GEOMETRY_PAGES> geometryPages;
    Graphics::RGResourceHandle                              materials;

    /// @brief Declares every global buffer as shader read
    void read( Graphics::RenderPassBuilder& pb ) { declare( pb, Graphics::RHI::ResourceState::ShaderResource, false ); }
    /// @brief Declares every global buffer as copy destination (upload)
    void write( Graphics::RenderPassBuilder& pb ) { declare( pb, Graphics::RHI::ResourceState::CopyDest, true ); }

private:
    void declare( Graphics::RenderPassBuilder& pb, Graphics::RHI::ResourceState state, bool isWrite ) {
        auto access = [&]( Graphics::RGResourceHandle& h ) { h = isWrite ? pb.write( h, state ) : pb.read( h, state ); };
        for ( auto& page : geometryPages )
            for ( auto* h : { &page.vertex, &page.index, &page.meshlets, &page.meshletVertices, &page.meshletPrimitives } )
                access( *h );
        access( materials );
    }
};

/// @brief Imports the geometry pages and the material buffer in this frame graph
GlobalBufferHandles importGlobalBuffers( Graphics::RenderGraphBuilder& builder, const GPUResources& res );

} // namespace Resources
} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END
