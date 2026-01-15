//Aligned to 16 bytes strides doe tto ConstantBuffer usage
struct FrameData {
    float4x4 viewProj;
    float4x4 invProj;
    float4x4 invView;

    float4 camPos_Time;  // .xyz = Pos, .w = Time
    float4 res_Clip;     // .xy = Res, .zw = Near/Far
    float4 sceneParams;  // .x = LightCount, .y = InstanceCount 
    float4 sceneAABBMin; // .xyz = Min
    float4 sceneAABBMax; // .xyz = Max
};

float3 getCameraPos( FrameData f ) { return f.camPos_Time.xyz; }
float  getTime( FrameData f ) { return f.camPos_Time.w; }

float2 getResolution( FrameData f ) { return f.res_Clip.xy; }
float2 getClippingPlanes( FrameData f ) { return f.res_Clip.zw; } // x=Near, y=Far

uint getLightCount( FrameData f ) { return (uint)f.sceneParams.x; }
uint getInstanceCount( FrameData f ) { return (uint)f.sceneParams.y; }

float3 getAABBMin( FrameData f ) { return f.sceneAABBMin.xyz; }
float3 getAABBMax( FrameData f ) { return f.sceneAABBMax.xyz; }

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
