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
    float4x4 modelMatrix;
    float4x4 normalMatrix;
    uint     materialID;
    uint     meshID;
    uint     active;
    uint     raytraced;
};

struct MeshData {
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint indexCount;

    float4 bsphere;

    float4 aabbMin; // .w unused
    float4 aabbMax; // .w unused

    uint needsAS;
    uint valid;
    uint lastFrameUsed;
    uint originalAssetID;
};

struct LightData {
    float4 pos_Intensity; // .xyz = Pos, .w = Intensity
    float4 col_Radius;    // .xyz = Col, .w = Radius
    float4 dir_Area;      // .xyz = Nrm, .w = Area
    float4 settings;      // .x = Active
};

float3 getLightPos( LightData l ) { return l.pos_Intensity.xyz; }
float  getLightIntensity( LightData l ) { return l.pos_Intensity.w; }
float3 getLightCol( LightData l ) { return l.col_Radius.xyz; }
float  getLightRadius( LightData l ) { return l.col_Radius.w; }