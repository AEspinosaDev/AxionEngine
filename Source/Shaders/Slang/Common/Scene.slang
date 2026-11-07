// MODULE: GPU scene data types (mirror of GPUObjects.h). import it.
// ------------------------------------------------------------------
// FRAME DATA (Global Per-Frame Constants)
// ------------------------------------------------------------------
// Aligned to 16 bytes for ConstantBuffer usage
public struct FrameData {
    // --- Raw Data ---
    public float4x4 viewProj;
    public float4x4 jitteredViewProj;
    public float4x4 prevViewProj;
    public float4x4 invProj;
    public float4x4 invView;

    public float4 camPos_Time;  // .xyz = Pos, .w = Time
    public float4 res_Clip;     // .xy = Res, .zw = Near/Far
    public float4 sceneParams;  // .x = LightCount, .y = InstanceCount, .z = enviromentCount
    public float4 sceneAABBMin; // .xyz = Min (w unused)
    public float4 sceneAABBMax; // .xyz = Max (w unused)

    public float4 frustrumPlanes[6];

    public uint fallbackTex2DID;  // Bindless defaults (renderer)
    public uint linearSamplerID;
    public uint materialBufferID; // Global material payloads (ByteAddressBuffer)
    public uint _pad;

    // --- Helper Methods (Getters) ---

    public float3 getCameraPos() { return camPos_Time.xyz; }
    public float  getTime() { return camPos_Time.w; }

    public float2 getResolution() { return res_Clip.xy; }
    public float2 getClippingPlanes() { return res_Clip.zw; } // x=Near, y=Far

    public uint getLightCount() { return (uint)sceneParams.x; }
    public uint getInstanceCount() { return (uint)sceneParams.y; }
    public uint getEnvCount() { return (uint)sceneParams.z; }

    public float3 getAABBMin() { return sceneAABBMin.xyz; }
    public float3 getAABBMax() { return sceneAABBMax.xyz; }
};

// ------------------------------------------------------------------
// INSTANCE DATA (Per-Object)
// ------------------------------------------------------------------
public struct InstanceData {
    public float4x4 modelMatrix;
    public float4x4 prevModelMatrix;
    public float4x4 normalMatrix;
    public uint     meshID;
    public uint     materialID;
    public uint     active;
    public uint     raytraced;
};

// ------------------------------------------------------------------
// PARTICLE SYSTEM DATA (Per-System)
// ------------------------------------------------------------------
public static const uint PARTICLE_BEHAVIOR_ORBITALS = 0;

public struct ParticleSystemData {
    public float4x4 modelMatrix;

    public uint meshID;
    public uint materialID;
    public uint count;
    public uint bufferSRVHeapID; // Read by the drawing

    public float4 extent_Size; // .xyz = Extent, .w = Size
    public float  speed;
    public uint   atomicity;
    public uint   behavior;
    public uint   seed;

    public uint visible;
    public uint update;
    public uint reset;
    public uint faceCamera;

    public uint bufferUAVHeapID;      // Written by the simulation
    public uint bufferSlot;           // CPU bookkeeping, unused by shaders
    public uint visibleListSRVHeapID; // Visible particles. Read by the drawing
    public uint visibleListUAVHeapID; // Visible particles. Written by the culling

    public float sizeVariation; // Each particle is seeded between size / variation and size * variation
    public uint  _pad0;
    public uint  _pad1;
    public uint  _pad2;

    public float3 getExtent() { return extent_Size.xyz; }
    public float  getSize() { return extent_Size.w; }
};

// State of one simulated particle: element of a particle buffer. GPU only.
// 48 bytes: must match Config::PARTICLE_DATA_STRIDE
public struct ParticleData {
    public float3 position;      // Entity space
    public float  scale;         // Over the size of the system
    public float4 orientation;   // Unit quaternion
    public float4 behaviorState; // Depends on the behavior
};

// ------------------------------------------------------------------
// MESH DATA (Geometry Metadata)
// ------------------------------------------------------------------
public struct MeshData {
    // Geometry page buffers (bindless heap IDs, INVALID until the mesh is resident)
    public uint vertexHeapID;
    public uint indexHeapID;
    public uint meshletHeapID;
    public uint meshletVertexHeapID;

    public uint meshletPrimitiveHeapID;
    public uint geometryPageID;       // CPU bookkeeping, unused by shaders
    public uint meshletVertexCount;   // CPU bookkeeping, unused by shaders
    public uint meshletPrimitiveSize; // CPU bookkeeping, unused by shaders

    // Byte offsets into the page buffers
    public uint vertexOffset;
    public uint indexOffset;
    public uint vertexCount;
    public uint indexCount;

    public float4 bsphere; // xyz = center, w = radius

    public float4 aabbMin;         // .w unused
    public float4 aabbMaxTopology; // .w Topology

    public uint needsAS;
    public uint valid;
    public uint lastFrameUsed;
    public uint originalAssetID;

    // Meshlets: byte offsets into the page meshlet / meshlet vertex / meshlet primitive buffers
    public uint meshletOffset;
    public uint meshletVertexOffset;
    public uint meshletPrimitiveOffset;
    public uint meshletCount;

    // --- Helpers ---
    public uint getTopology() { return (uint)aabbMaxTopology.w; }

    public float3 getAABBMin() { return aabbMin.xyz; }
    public float3 getAABBMax() { return aabbMaxTopology.xyz; }
};

// ------------------------------------------------------------------
// MATERIAL DATA (Header only)
// ------------------------------------------------------------------
public struct MaterialData {
    public uint bufferOffset; // Pointer to the big ByteAddressBuffer
    public uint payloadSize;
    public uint psoBundleID; // MaterialLibrary pipeline bundle (archetype + render state)
    public uint valid;

    public uint lastFrameUsed;
    public uint originalAssetID;
    public uint uv[2];
};

// ------------------------------------------------------------------
// TEXTURE DATA (GPUScene texture cache: payloads store the cache slot)
// ------------------------------------------------------------------
public struct TextureData {
    public uint heapID; // Bindless index in the global heap (INVALID while not uploaded)
    public uint valid;
    public uint refCount;
    public uint originalAssetID;
};

// ------------------------------------------------------------------
// LIGHT DATA
// ------------------------------------------------------------------

public static const uint LIGHT_TYPE_DIR   = 0;
public static const uint LIGHT_TYPE_POINT = 1;
public static const uint LIGHT_TYPE_SPOT  = 2;
public static const uint LIGHT_TYPE_AREA  = 3;

public struct LightData {
    public float4 pos_Intensity;
    public float4 col_Range;
    public float4 dir_Type;
    public float4 params;

    // --- Getters Semánticos ---

    public property float3 position {
        get { return pos_Intensity.xyz; }
    }
    public property float intensity {
        get { return pos_Intensity.w; }
    }
    public property float3 color {
        get { return col_Range.xyz; }
    }
    public property float range {
        get { return col_Range.w; }
    }
    public property float3 direction {
        get { return dir_Type.xyz; }
    } // Directional/Spot only
    public property uint type {
        get { return (uint)dir_Type.w; }
    }
    public property bool isActive {
        get { return params.w > 0.5; }
    }

    public property float spotInnerCos {
        get { return params.x; }
    }
    public property float spotOuterCos {
        get { return params.y; }
    }

    // property float2 areaSize { get { return params.zw; } } // Width/Height reutilizando slots
};

// ------------------------------------------------------------------
// ENVIROMENT DATA
// ------------------------------------------------------------------

public static const uint ENV_TYPE_CONST = 0;
public static const uint ENV_TYPE_PROC  = 1;
public static const uint ENV_TYPE_HDRI  = 2;

public struct EnvData {
    public float4 skyColor_Intensity;
    public float4 groundColor_Type;
    public uint   colorCubeMapID;
    public uint   irradianceMapID;
    public uint   prefilteredMapID;
    public uint   brdfLUTID;
    public float4 rotation_Blend;
    public float4 procSettings;

    public property float3 skyColor {
        get { return skyColor_Intensity.xyz; }
    }
    public property float3 groundColor {
        get { return groundColor_Type.xyz; }
    }
    public property float intensity {
        get { return skyColor_Intensity.w; }
    }
    public property uint type {
        get { return (uint)groundColor_Type.w; }
    }
};
