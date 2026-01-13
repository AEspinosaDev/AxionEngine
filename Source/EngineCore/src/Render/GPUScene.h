#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/RHI/Resource.h"
#include "queue"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Flags to control the data transformation pipeline during the update.
enum GPUSceneUpdateFlags : uint
{
    GPUSceneNone              = 1 << 0,
    GPUSceneForceRaytrace     = 1 << 1, // Force all objects to be marked for Raytracing AS
    GPUSceneTransposeMatrices = 1 << 2, // Required for DX12 (Row-Major vs Col-Major mismatch)
    GPUSceneForgetCache       = 1 << 3, // Force a full cache rebuild (useful for level reload)
};
AXION_ENUM_CLASS_FLAG_OPERATORS( GPUSceneUpdateFlags );

// -----------------------------------------------------------------------------
// TRANSIENT DATA (PER-FRAME)
// -----------------------------------------------------------------------------
// These structures represent the linear "stream" of data for the current frame.
// They are cleared and rebuilt every Update(). Designed for linear GPU access (StructuredBuffers).

struct GPUInstance {
    Math::Mat4 modelMatrix;  // 64 bytes
    Math::Mat4 normalMatrix; // 64 bytes
    uint       materialID;   // Index or Offset into the Material Blob/Buffer
    uint       meshID;       // Index into the GPUMesh Cache (NOT the Asset ID)
    uint       active;       // Visibility flag
    uint       raytraced;    // Flag for TLAS inclusion
};

struct GPUFrame {
    Math::Mat4 viewProj;
    Math::Mat4 invProj;
    Math::Mat4 invView;
    Math::Vec3 cameraPosition;
    float      time;
    Math::Vec2 clippingPlanes;
    Extent2D   resolution;
    uint       lightCount;
    uint       instanceCount;
    Math::AABB sceneAABB; // Conservative bounds of the visible scene
};

struct GPULight {
    Math::Vec3 position;
    Math::Vec3 color;
    Math::Vec3 normal; // Direction for spot/directional lights
    float      intensity;
    float      radius; // Attenuation radius
    float      area;   // For Area Lights / Soft Shadows
    uint       active;
    float      padding[3]; // Padding to align to 16 bytes (float4) for GPU
};

// -----------------------------------------------------------------------------
// PERSISTENT DATA (CACHE METADATA)
// -----------------------------------------------------------------------------
// Represents a geometry slot in VRAM.
// NOTE: This does NOT contain the vertex data itself. It acts as a descriptor/view
// telling the Renderer WHERE in the MegaBuffer the data is located.
struct GPUMesh {
    // Memory layout in the Global Geometry Buffer
    uint vertexOffset = 0;
    uint indexOffset  = 0;
    uint vertexCount  = 0;
    uint indexCount   = 0;

    // Spatial data for Culling/Raytracing
    Math::BoundingSphere bsphere;
    Math::AABB           aabb;

    bool needsAS = false; // Flag to build BLAS (Bottom Level Acceleration Structure)
    bool valid   = false; // True if data is uploaded and resident in VRAM

    // Garbage Collection Tracker (Time-To-Live)
    uint lastFrameUsed = 0;

    // Back-reference to CPU Asset for cache invalidation
    uint originalAssetID = 0;
};

// -----------------------------------------------------------------------------
// MESSAGE QUEUES (RHI COMMANDS)
// -----------------------------------------------------------------------------
// GPUScene cannot allocate GPU memory directly (it's API agnostic).
// It uses these structures to send "Orders" to the Renderer/Allocator.

// Order: "Please upload this raw CPU data to VRAM and tell me the offsets"
struct PendingMeshEntry {
    uint                        GPUMeshID; // Destination Slot in _meshCache
    std::vector<Assets::Vertex> vertices;  // Raw data copy (safe against asset unloading)
    std::vector<uint>           indices;
};

// Order: "This slot is empty, please mark this VRAM region as free"
struct PendingMeshFree {
    uint vertexOffset;
    uint vertexSize;
    uint indexOffset;
    uint indexSize;
    uint GPUMeshID; // Slot to recycle
};

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
    struct TransientOffsets {
        uint frameOffset    = 0;
        uint meshOffset     = 0;
        uint instanceOffset = 0;
        uint lightOffset    = 0;
    };

    GPUScene()  = default;
    ~GPUScene() = default;

    // -- Read-Write Accessors for the Renderer --
    // The Renderer consumes these vectors to fill the Volatile Allocators (LinearAllocators)
    GPUFrame&                 frame() { return _frame; }
    std::vector<GPUInstance>& instances() { return _instances; }
    std::vector<GPULight>&    lights() { return _lights; }

    // Persistent cache access (Used to bind SRVs for geometry)
    std::vector<GPUMesh>& meshes() { return _meshCache; }

    // Command Queues consumption
    std::queue<PendingMeshEntry>& pendingMeshUploads() { return _pendingMeshUploads; }
    std::queue<PendingMeshFree>&  pendingMeshReleases() { return _pendingMeshReleases; }

    // Query
    bool hasPendingUploads() const;
    bool hasPendingReleases() const;

    /**
     * @brief Main processing function. Rebuilds the transient data for the current frame.
     * @param cpuScene Source of truth (ECS Registry).
     * @param cameraEntity The point of view for this render pass (Culling/ViewProj).
     * @param flags Modifiers for the update pipeline (e.g., DX12 Transpose).
     */
    void update( const Scene::Scene& cpuScene,
                 Scene::Entity&      cameraEntity,
                 const Extent2D&     resolution,
                 float               deltaTime,
                 GPUSceneUpdateFlags flags = GPUSceneNone );

    /**
     * @brief Uploads the transient data to the main UBO buffer.
     * @param destBuffer Raw pointer to a CPU VISIBLE RHI::IBuffer that handles all the transient uniforms.
     */
    TransientOffsets uploadTransientData( Graphics::RHI::IBuffer* destBuffer );

    void setGCMode( Graphics::GCMode mode ) { _resourceTTL = (uint)mode; }

private:
    // -- Internal Pipeline Stages --
    void reset( float dt );
    void processMeshes( const Scene::Scene& cpuScene, bool transpose, bool forceRaytrace );
    void processLights( const Scene::Scene& cpuScene );
    void processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose );
    void runGC();

    // -- Transient Data (Cleared every frame) --
    GPUFrame                 _frame;
    std::vector<GPUInstance> _instances;
    std::vector<GPULight>    _lights;

    // -- Persistent Data Cache --
    // The slot container. Indices here are stable until GC.
    std::vector<GPUMesh> _meshCache;
    // Slots that were freed and can be reused.
    std::queue<ulong> _freeIndexQueue;
    // O(1) Look-Up Table mapping [CPU_AssetID] -> [GPU_CacheSlot]
    std::vector<int> _assetToCacheLUT;

    // -- Communication Queues --
    std::queue<PendingMeshEntry> _pendingMeshUploads;
    std::queue<PendingMeshFree>  _pendingMeshReleases;

    // -- State --
    bool  _forceRaytrace     = false;
    float _accumulatedTime   = 0.0f;
    uint  _currentFrameIndex = 0;

    uint _resourceTTL = (uint)Graphics::GCMode::AvgMemory;
};

} // namespace Core::Render

AXION_NAMESPACE_END