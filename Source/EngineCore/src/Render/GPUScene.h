#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include "Axion/Graphics/Handle.h"
#include "GPUObjects.h"
#include "MaterialSystem.h"
#include "queue"
#include <span>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Flags to control the data transformation pipeline during the update.
enum GPUSceneUpdateFlags : uint
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
    GPUFrame&                 frame() { return _frame; }
    std::vector<GPUInstance>& instances() { return _instances; }
    std::vector<GPULight>&    lights() { return _lights; }

    // Persistent cache access (Used to bind SRVs for geometry)
    std::vector<GPUMesh>& meshes() { return _meshCache.cache; }
    // Persistent cache access (Used to bind SRVs for materials)
    std::vector<GPUMaterial>& materials() { return _materialCache.cache; }

    // Command Queues consumption
    std::queue<PendingMeshUpload>&     pendingMeshUploads() { return _pendingMeshUploads; }
    std::queue<PendingMeshFree>&       pendingMeshReleases() { return _pendingMeshReleases; }
    std::queue<PendingMaterialUpload>& pendingMaterialUploads() { return _pendingMtlUploads; }
    std::queue<PendingMaterialFree>&   pendingMaterialReleases() { return _pendingMtlReleases; }

    struct SortKey {
        ulong key;
        uint  originalInstanceIdx;

        void unpack( uint& archID, uint& topology, uint& meshID ) const {
            archID   = (uint)( ( key >> 48 ) & 0xFFFF );
            topology = (uint)( ( key >> 44 ) & 0xF );
            meshID   = (uint)( key & 0xFFFFFFFFFFF );
        }
    };

    const std::vector<SortKey>& getSortedKeys() const { return _sortedKeys; }

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

    void setGCMode( Graphics::GCMode mode ) { _resourceTTL = (uint)mode; }

private:
    // -- Internal Pipeline Stages --
    void reset( float dt );

    void processInstances( const Scene::Scene&    cpuScene,
                           const MaterialLibrary& mtlLib,
                           bool                   sort,
                           bool                   transpose,
                           bool                   forceRaytrace );
    uint processMesh( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::MeshHandle& cpuMeshHandle );
    uint processMaterial( const Axion::Core::Assets::AssetManager*   assets,
                          const MaterialLibrary&                     mtlLib,
                          std::pair<const uchar*, size_t>&           dirtyLUT,
                          const Axion::Core::Assets::MaterialHandle& cpuMtlHandle );

    void processLights( const Scene::Scene& cpuScene );
    void processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose );
    void runGC();

    AXION_FORCE_INLINE ulong makeSortKey( uint archID, uint topology, uint meshID ) {
        // [Archetype 16b] [Topology 4b] [Mesh 44b]
        return ( (ulong)archID << 48 ) |
               ( (ulong)topology << 44 ) |
               ( (ulong)meshID & 0xFFFFFFFFFFF );
    }

    // -- Transient Data (Cleared every frame) --
    GPUFrame                 _frame;
    std::vector<GPUInstance> _instances;
    std::vector<GPULight>    _lights;

    std::vector<SortKey> _sortedKeys;

    // -- Persistent Data Cache --
    template <typename T>
    struct GPUCache {
        // The slot container. Indices here are stable until GC.
        std::vector<T> cache;
        // Slots that were freed and can be reused.
        std::queue<ulong> freeIndexQueue;
        // O(1) Look-Up Table mapping [CPU_AssetID] -> [GPU_CacheSlot]
        std::vector<int> assetToCacheLUT;
    };
    GPUCache<GPUMesh>     _meshCache;
    GPUCache<GPUMaterial> _materialCache;

    // -- Communication Queues --
    std::queue<PendingMeshUpload>     _pendingMeshUploads;
    std::queue<PendingMeshFree>       _pendingMeshReleases;
    std::queue<PendingMaterialUpload> _pendingMtlUploads;
    std::queue<PendingMaterialFree>   _pendingMtlReleases;

    // -- State --
    bool  _forceRaytrace     = false;
    float _accumulatedTime   = 0.0f;
    uint  _currentFrameIndex = 0;

    uint _resourceTTL = (uint)Graphics::GCMode::AvgMemory;
};

} // namespace Core::Render

AXION_NAMESPACE_END