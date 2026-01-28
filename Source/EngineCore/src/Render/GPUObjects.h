#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Common/Math.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// -----------------------------------------------------------------------------
// TRANSIENT DATA (PER-FRAME)
// -----------------------------------------------------------------------------
// These structures represent the linear "stream" of data for the current frame.
// They are cleared and rebuilt every Update(). Designed for linear GPU access (StructuredBuffers).

struct GPUInstance {
    Math::Mat4 modelMatrix;  // 64 bytes
    Math::Mat4 normalMatrix; // 64 bytes
    uint       meshID;       // 4 bytes
    uint       materialID;   // 4 bytes
    uint       active;       // 4 bytes
    uint       raytraced;    // 4 bytes

    // Total: 144 bytes. (Multiple of 4, OK for StructuredBuffer).
};

struct GPUFrame {
    Math::Mat4 viewProj;
    Math::Mat4 invProj;
    Math::Mat4 invView;
    Math::Vec4 camPos_Time;  // Packed: x,y,z = camPos | w = time
    Math::Vec4 res_Clip;     // Packed: x,y = resolution | z,w = clippingPlanes (near, far)
    Math::Vec4 sceneParams;  // Packed: x = lightCount | y = instanceCount | z,w = padding (unsused)
    Math::Vec4 sceneAABBMin; // x,y,z = AABB Min | w = padding
    Math::Vec4 sceneAABBMax; // x,y,z = AABB Max | w = padding
    Math::Vec4 frustrumPlanes[6];
};

struct GPULight {

    Math::Vec4 pos_Intensity; // Packed: xyz = Position, w = Intensity
    Math::Vec4 col_Radius;    // Packed: xyz = Color, w = Radius
    Math::Vec4 dir_Area;      // Packed: xyz = Direction/Normal, w = Area
    Math::Vec4 settings;      // Settings: x = active. y,z,w = padding (unused)

    // Total: 64 bytes. (Multiple of 4, OK for StructuredBuffer).
};

// -----------------------------------------------------------------------------
// PERSISTENT DATA (CACHE METADATA)
// -----------------------------------------------------------------------------
// Represents a geometry slot in VRAM.
// NOTE: This does NOT contain the vertex data itself. It acts as a descriptor/view
// telling the Renderer WHERE in the MegaBuffer the data is located.
struct GPUMesh {
    // Offsets y Counts (16 bytes)
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint indexCount;

    Math::Vec4 bsphere;          // Bounding Sphere (16 bytes) -> xyz = center, w = radius
    Math::Vec4 aabbMin;          // AABB Min (16 bytes) -> xyz = min, w = unused
    Math::Vec4 aabbMax_Topology; // AABB Max (16 bytes) -> xyz = max, w = topology ID

    // Flags y Tracking (16 bytes)
    uint needsAS;
    uint valid;
    uint lastFrameUsed;
    uint originalAssetID;

    // Total: 80 bytes. (Multiple of 4, OK for StructuredBuffer).
};

struct GPUMaterial {
    // Offsets y Counts (16 bytes)
    uint bufferOffset;
    uint payloadSize;
    uint lastFrameUsed;
    uint archetypeID;
    // Maybe I could aadd here basic ovverides (albedo, uv, etc) that dont need the slow staging route
};

// -----------------------------------------------------------------------------
// MESSAGE QUEUES (RHI COMMANDS)
// -----------------------------------------------------------------------------
// GPUScene cannot allocate GPU memory directly (it's API agnostic).
// It uses these structures to send "Orders" to the Renderer/Allocator.

// Order: "Please upload this raw CPU data to VRAM and tell me the offsets"
struct PendingMeshUpload {
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

// // Order: "Please upload this raw CPU data to VRAM and tell me the offsets"
struct PendingMaterialUpload {
    uint               GPUMaterialID;
    std::vector<uchar> payload;
};

// // Order: "This slot is empty, please mark this VRAM region as free"
struct PendingMaterialFree {
    uint bufferOffset;
    uint payloadSize;
    uint GPUMaterialID;
};


} // namespace Core::Render

AXION_NAMESPACE_END