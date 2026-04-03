#pragma once
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
    u32       meshID;       // 4 bytes
    u32       materialID;   // 4 bytes
    u32       active;       // 4 bytes
    u32       raytraced;    // 4 bytes

    // Total: 144 bytes. (Multiple of 4, OK for StructuredBuffer).
};

struct GPUEnvironment {
    Math::Vec4 skyColor_Intensity;
    Math::Vec4 groundColor_Type;
    u32       colorCubeMapID   = 0xFFFFFFFF;
    u32       irradianceMapID  = 0xFFFFFFFF;
    u32       prefilteredMapID = 0xFFFFFFFF;
    u32       brdfLUTID        = 0xFFFFFFFF;
    Math::Vec4 rotation_BlendDist;
    Math::Vec4 procSettings;
};

struct GPUFrame {
    Math::Mat4 viewProj;
    Math::Mat4 invProj;
    Math::Mat4 invView;
    Math::Vec4 camPos_Time;  // Packed: x,y,z = camPos | w = time
    Math::Vec4 res_Clip;     // Packed: x,y = resolution | z,w = clippingPlanes (near, far)
    Math::Vec4 sceneParams;  // Packed: x = lightCount | y = instanceCount | z = enviromentCount | w = padding (unsused)
    Math::Vec4 sceneAABBMin; // x,y,z = AABB Min | w = padding
    Math::Vec4 sceneAABBMax; // x,y,z = AABB Max | w = padding
    Math::Vec4 frustrumPlanes[6];
};

struct GPULight {

    Math::Vec4 pos_Intensity; // Packed: xyz = Position, w = Intensity
    Math::Vec4 col_Radius;    // Packed: xyz = Color, w = Radius
    Math::Vec4 dir_Type;      // Packed: xyz = Direction/Normal, w = Area
    // x = Spot Inner Angle Cosine
    // y = Spot Outer Angle Cosine
    // z = ShadowMap Index (or Area Light Width)
    // w = Active flag (or Area Light Height)
    Math::Vec4 settings;
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
    u32 vertexOffset;
    u32 indexOffset;
    u32 vertexCount;
    u32 indexCount;

    Math::Vec4 bsphere;          // Bounding Sphere (16 bytes) -> xyz = center, w = radius
    Math::Vec4 aabbMin;          // AABB Min (16 bytes) -> xyz = min, w = unused
    Math::Vec4 aabbMax_Topology; // AABB Max (16 bytes) -> xyz = max, w = topology ID

    // Flags y Tracking (16 bytes)
    u32 needsAS;
    u32 valid;
    u32 lastFrameUsed;
    u32 originalAssetID;

    // Total: 80 bytes. (Multiple of 4, OK for StructuredBuffer).
};

struct GPUMaterial {
    // Offsets y Counts (32 bytes)
    u32 bufferOffset;
    u32 payloadSize;
    u32 archetypeID;
    u32 valid;

    u32 lastFrameUsed;
    u32 originalAssetID;
    u32 uv[2];
    // Maybe I could aadd here basic ovverides (albedo, uv, etc) that dont need the slow staging route
};

struct GPUTexture {
    u32 slot;

    u32 valid;
    u32 lastFrameUsed;
    u32 originalAssetID;
};

// -----------------------------------------------------------------------------
// MESSAGE QUEUES (RHI COMMANDS)
// -----------------------------------------------------------------------------
// GPUScene cannot allocate GPU memory directly (it's API agnostic).
// It uses these structures to send "Orders" to the Renderer/Allocator.

// Order: "Please upload this raw CPU data to VRAM and tell me the offsets"
struct PendingMeshUpload {
    u32                                  GPUMeshID;    // Destination Slot in _meshCache
    std::shared_ptr<Assets::GeometryData> geometryData; // Optional meshlet data for AS-capable meshes
};

// Order: "This slot is empty, please mark this VRAM region as free"
struct PendingMeshFree {
    u32 vertexOffset;
    u32 vertexSize;
    u32 indexOffset;
    u32 indexSize;
    u32 GPUMeshID; // Slot to recycle
};

// // Order: "Please upload this raw CPU data to VRAM and tell me the offsets"
struct PendingMaterialUpload {
    u32                GPUMaterialID;
    STLW::Vector<byte> payload;
};

// // Order: "This slot is empty, please mark this VRAM region as free"
struct PendingMaterialFree {
    u32 bufferOffset;
    u32 payloadSize;
    u32 GPUMaterialID;
};

struct PendingTextureUpload {
    u32                                   slot;
    std::shared_ptr<Assets::TexturePixels> pixels;
    Graphics::Format                       format;
    Extent3D                               extent;
    String64                               name;
};

} // namespace Core::Render

AXION_NAMESPACE_END