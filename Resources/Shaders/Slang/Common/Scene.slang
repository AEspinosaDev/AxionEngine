
struct FrameData {
    float4x4 viewProj;
    float4x4 invProj;
    float4x4 invView;
    float3   cameraPosition;
    float    time;
    float2   clippingPlanes;
    float2   resolution;
    uint     lightCount;
    uint     instanceCount;
    float3   sceneAABBMin; // AABB desglosada para padding fácil
    float3   sceneAABBMax;
};

struct InstanceData {
    float4x4 modelMatrix;  // 64 bytes
    float4x4 normalMatrix; // 64 bytes
    uint     materialID;   // 4 bytes
    uint     meshID;       // 4 bytes (Index into gMeshes)
    uint     active;       // 4 bytes
    uint     raytraced;    // 4 bytes
    // Total: 144 bytes. (144 % 16 != 0), ojo con padding si usas arrays muy compactos,
};

struct MeshData {
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint indexCount;

    float4 bsphere;

    float3 aabbMin;
    float3 aabbMax;

    uint needsAS;
    uint valid;
    uint lastFrameUsed;
    uint originalAssetID;
};

struct LightData {
    float3 position;
    float  padding0; // Slang/HLSL alinea float3+float a float4
    float3 color;
    float  padding1;
    float3 normal;
    float  intensity;
    float  radius;
    float  area;
    uint   active;
    float  padding2;
    // Asegúrate de que esto suma lo mismo que tu struct C++ con padding[3]
};
