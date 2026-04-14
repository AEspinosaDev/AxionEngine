#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include "Axion/Graphics/Handle.h"
#include "GPUObjects.h"
#include "MaterialSystem.h"
#include <Axion/Common/Containers/STLWrapper/Lists.h>
#include <span>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Flags to control the data transformation pipeline during the update.
enum GPUSceneUpdateFlags : u32
{
    GPUSceneNone              = 1 << 0,
    GPUSceneForceRaytrace     = 1 << 1, // Force all objects to be marked for Raytracing AS
    GPUSceneTransposeMatrices = 1 << 2, // Required for DX12 (Row-Major vs Col-Major mismatch)
    GPUSceneForgetCache       = 1 << 3, // Force a full cache rebuild (useful for level reload)
    GPUSceneSortInstances     = 1 << 4, // Instances will be sorted by material archetype, primitive and distance if needed
};

AXION_ENUM_CLASS_FLAG_OPERATORS( GPUSceneUpdateFlags );

// -----------------------------------------------------------------------------
// GPU SCENE MIDDLEWARE
// -----------------------------------------------------------------------------
/**
 * @brief Acts as a bridge between high-level ECS logic and low-level GPU buffers.
 * * Responsibilities:
 * 1. Data Transformation: Converts ECS Components to linear GPU-ready structs (PODs).
 * 2. Cache Management: Maps CPU Asset IDs to GPU Slots using O(1) LUTs.
 * 3. Memory Lifecycle: Detects unused assets (GC) and requests Uploads/Releases via queues.
 * * @note This class is Renderer-Agnostic. It prepares data, it does not issue Draw Calls.
 */
class GPUScene
{
public:
    GPUScene()  = default;
    ~GPUScene() = default;

    // -- Read-Write Accessors for the Renderer --
    // The Renderer consumes these vectors to fill the Volatile Allocators (LinearAllocators)
    GPUFrame&               frame() { return _frame; }
    Vector<GPUInstance>&    instances() { return _instances; }
    Vector<GPULight>&       lights() { return _lights; }
    Vector<GPUEnvironment>& environments() { return _environments; }

    // Persistent cache access (Used to bind SRVs for geometry)
    STLW::Vector<GPUMesh>& meshes() { return _meshCache.cache; }
    // Persistent cache access (Used to bind SRVs for materials)
    STLW::Vector<GPUMaterial>& materials() { return _materialCache.cache; }
    // Persistent cache access (Used to bind SRVs for textures)
    STLW::Vector<GPUTexture>& textures() { return _textureCache.cache; }

    // Command Queues consumption
    STLW::Queue<PendingMeshUpload>&     pendingMeshUploads() { return _pendingMeshUploads; }
    STLW::Queue<PendingMeshFree>&       pendingMeshReleases() { return _pendingMeshReleases; }
    STLW::Queue<PendingMaterialUpload>& pendingMaterialUploads() { return _pendingMtlUploads; }
    STLW::Queue<PendingMaterialFree>&   pendingMaterialReleases() { return _pendingMtlReleases; }
    STLW::Queue<PendingTextureUpload>&  pendingTextureUploads() { return _pendingTextureUploads; }
    STLW::Queue<u32>&                   pendingTextureReleases() { return _pendingTextureReleases; }

    struct SortKey {
        u64 key;
        u32 originalInstanceIdx;

        void unpack( u32& archID, u32& topology, u32& meshID ) const {
            archID   = (u32)( ( key >> 48 ) & 0xFFFF );
            topology = (u32)( ( key >> 44 ) & 0xF );
            meshID   = (u32)( key & 0xFFFFFFFFFFF );
        }
    };

    const Vector<SortKey>& getSortedKeys() const { return _sortedKeys; }

    // Query
    bool hasPendingUploads() const;
    bool hasPendingReleases() const;

    /**
     * @brief Main processing function. Rebuilds the transient data for the current frame.
     * @param cpuScene Source of truth (ECS Registry).
     * @param cameraEntity The point of view for this render pass (Culling/ViewProj).
     * @param flags Modifiers for the update pipeline (e.g., DX12 Transpose).
     */
    void update( const Scene::Scene&    cpuScene,
                 Scene::Entity&         cameraEntity,
                 const MaterialLibrary& mtlLib,
                 const Extent2D&        resolution,
                 float                  deltaTime,
                 GPUSceneUpdateFlags    flags = GPUSceneNone );

    void setGCMode( Graphics::GCMode mode ) { _resourceTTL = (u32)mode; }

private:
    // -- Internal Pipeline Stages --
    void reset( float dt );

    void processInstances( const Scene::Scene&    cpuScene,
                           const MaterialLibrary& mtlLib,
                           bool                   sort,
                           bool                   transpose,
                           bool                   forceRaytrace );
    u32  processMesh( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::MeshHandle& cpuMeshHandle );
    u32  processMaterial( const Axion::Core::Assets::AssetManager*   assets,
                          const MaterialLibrary&                     mtlLib,
                          std::pair<const byte*, size_t>&            dirtyLUT,
                          const Axion::Core::Assets::MaterialHandle& cpuMtlHandle );
    u32  processTexture( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::TextureHandle& cpuHandle, u32 materialGpuCacheIndex );

    void processLights( const Scene::Scene& cpuScene );
    void processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose );
    void processEnvironments( const Scene::Scene& cpuScene );

    void runGC();

    AXION_FORCE_INLINE u64 makeSortKey( u32 archID, u32 topology, u32 meshID ) {
        // [Archetype 16b] [Topology 4b] [Mesh 44b]
        return ( (u64)archID << 48 ) |
               ( (u64)topology << 44 ) |
               ( (u64)meshID & 0xFFFFFFFFFFF );
    }

    // -- Transient Data (Cleared every frame) --
    GPUFrame               _frame;
    Vector<GPUInstance>    _instances;
    Vector<GPULight>       _lights;
    Vector<GPUEnvironment> _environments;

    Vector<SortKey> _sortedKeys;

    // -- Persistent Data Cache --
    template <typename T>
    struct GPUCache {
        // The slot container. Indices here are stable until GC.
        STLW::Vector<T> cache;
        // Slots that were freed and can be reused.
        STLW::Queue<u64> freeIndexQueue;
        // O(1) Look-Up Table mapping [CPU_AssetID] -> [GPU_CacheSlot]
        STLW::Vector<int> assetToCacheLUT;
    };
    GPUCache<GPUMesh>     _meshCache;
    GPUCache<GPUMaterial> _materialCache;
    GPUCache<GPUTexture>  _textureCache;

    constexpr static u64                               MAX_STACK_TEXTURES = 6ull;
    STLW::Vector<SmallVector<u32, MAX_STACK_TEXTURES>> _materialToTextureMap;

    // -- Communication Queues --
    STLW::Queue<PendingMeshUpload>     _pendingMeshUploads;
    STLW::Queue<PendingMeshFree>       _pendingMeshReleases;
    STLW::Queue<PendingMaterialUpload> _pendingMtlUploads;
    STLW::Queue<PendingMaterialFree>   _pendingMtlReleases;
    STLW::Queue<PendingTextureUpload>  _pendingTextureUploads;
    STLW::Queue<u32>                   _pendingTextureReleases;

    // -- State --
    bool  _forceRaytrace     = false;
    float _accumulatedTime   = 0.0f;
    u32   _currentFrameIndex = 0;

    u32 _resourceTTL = (u32)Graphics::GCMode::AvgMemory;
};

} // namespace Core::Render

AXION_NAMESPACE_END