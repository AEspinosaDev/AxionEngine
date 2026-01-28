// ------------------------------------------------------------------
// FRAME DATA (Global Per-Frame Constants)
// ------------------------------------------------------------------
// Aligned to 16 bytes for ConstantBuffer usage
public struct FrameData 
{
    // --- Raw Data ---
    public float4x4 viewProj;
    public float4x4 invProj;
    public float4x4 invView;

    public float4 camPos_Time;  // .xyz = Pos, .w = Time
    public float4 res_Clip;     // .xy = Res, .zw = Near/Far
    public float4 sceneParams;  // .x = LightCount, .y = InstanceCount
    public float4 sceneAABBMin; // .xyz = Min (w unused)
    public float4 sceneAABBMax; // .xyz = Max (w unused)

    public float4 frustrum[6];

    // --- Helper Methods (Getters) ---
    
    public float3 getCameraPos() { return camPos_Time.xyz; }
    public float  getTime()      { return camPos_Time.w; }

    public float2 getResolution()     { return res_Clip.xy; }
    public float2 getClippingPlanes() { return res_Clip.zw; } // x=Near, y=Far

    public uint getLightCount()    { return (uint)sceneParams.x; }
    public uint getInstanceCount() { return (uint)sceneParams.y; }

    public float3 getAABBMin() { return sceneAABBMin.xyz; }
    public float3 getAABBMax() { return sceneAABBMax.xyz; }
};

// ------------------------------------------------------------------
// INSTANCE DATA (Per-Object)
// ------------------------------------------------------------------
public struct InstanceData 
{
    public float4x4 modelMatrix;
    public float4x4 normalMatrix;
    public uint     meshID;
    public uint     materialID;
    public uint     active;
    public uint     raytraced;
    
};

// ------------------------------------------------------------------
// MESH DATA (Geometry Metadata)
// ------------------------------------------------------------------
public struct MeshData 
{
    public uint vertexOffset;
    public uint indexOffset;
    public uint vertexCount;
    public uint indexCount;

    public float4 bsphere;       // xyz = center, w = radius

    public float4 aabbMin;       // .w unused
    public float4 aabbMaxTopology; // .w Topology

    public uint needsAS;
    public uint valid;
    public uint lastFrameUsed;
    public uint originalAssetID;

    // --- Helpers ---
    public uint getTopology() { return (uint)aabbMaxTopology.w; }
    
    public float3 getAABBMin() { return aabbMin.xyz; }
    public float3 getAABBMax() { return aabbMaxTopology.xyz; }
};

// ------------------------------------------------------------------
// MATERIAL DATA (Header only)
// ------------------------------------------------------------------
public struct MaterialData 
{
    public uint bufferOffset;  // Pointer to the big ByteAddressBuffer
    public uint payloadSize;
    public uint lastFrameUsed;
    public uint archetypeID;   // For validation or branching if needed
};

// ------------------------------------------------------------------
// LIGHT DATA
// ------------------------------------------------------------------
public struct LightData 
{
    public float4 pos_Intensity; // .xyz = Pos, .w = Intensity
    public float4 col_Radius;    // .xyz = Col, .w = Radius
    public float4 dir_Area;      // .xyz = Nrm, .w = Area
    public float4 settings;      // .x = Active

    // --- Helpers ---
    public float3 getPosition()  { return pos_Intensity.xyz; }
    public float  getIntensity() { return pos_Intensity.w; }
    
    public float3 getColor()     { return col_Radius.xyz; }
    public float  getRadius()    { return col_Radius.w; }
    
    public float3 getDirection() { return dir_Area.xyz; }
    public float  getArea()      { return dir_Area.w; } // For area lights

    public bool isActive()       { return settings.x > 0.5; }
};