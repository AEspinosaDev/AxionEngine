#include "Defines.slang"
#include "Scene.slang"

// ============================================================================
// SPACE 0: GLOBAL BINDLESS (Persistent Resources)
// ============================================================================
public ByteAddressBuffer gGlobalVertices : register( t0, space0 );
public ByteAddressBuffer gGlobalIndices : register( t1, space0 );
public ByteAddressBuffer gGlobalMaterials : register( t2, space0 );
// Bindless Texture Pools
public Texture2D   gTextures2D[] : register( t3, space0 );
public Texture3D   gTextures3D[] : register( t4, space0 );
public TextureCube gTexturesCube[] : register( t5, space0 );

public SamplerState gSamplers[] : register( s0, space0 );

// ============================================================================
// SPACE 1: FRAME DATA (Updated Per-Frame)
// ============================================================================

public ConstantBuffer<FrameData>      gFrame : register( b0, space1 );
public StructuredBuffer<MeshData>     gMeshes : register( t0, space1 );
public StructuredBuffer<MaterialData> gMaterials : register( t1, space1 );
public StructuredBuffer<InstanceData> gInstances : register( t2, space1 );
public StructuredBuffer<LightData>    gLights : register( t3, space1 );
public StructuredBuffer<EnvData>      gEnvs : register( t4, space1 );
public StructuredBuffer<uint>         gInstanceRedirection : register( t5, space1 );

// ============================================================================
// SPACE 2: PASS I/O (Transient Resources)
// ============================================================================

// Read-Only
public Texture2D<float4> gPassRead2D_F4[] : register( t0, space2 );
public Texture2D<float>  gPassRead2D_F1[] : register( t1, space2 );
public Texture2D<uint2>  gPassRead2D_U2[] : register( t2, space2 );
public Texture2D<uint>   gPassRead2D_U1[] : register( t3, space2 );
public Texture3D<float4> gPassRead3D_F4[] : register( t4, space2 );
public Texture3D<float>  gPassRead3D_F1[] : register( t5, space2 );
public TextureCube<float4> gPassReadCube_F4[] : register( t6, space2 );
public ByteAddressBuffer gPassReadBuffer[] : register( t7, space2 );

// Read-Write
public RWTexture2D<float4> gPassWrite2D_F4[] : register( u0, space2 );
public RWTexture2D<float>  gPassWrite2D_F1[] : register( u1, space2 );
public RWTexture2D<uint2>  gPassWrite2D_U2[] : register( u2, space2 );
public RWTexture2D<uint>   gPassWrite2D_U1[] : register( u3, space2 );
public RWTexture3D<float4> gPassWrite3D_F4[] : register( u4, space2 );
public RWTexture3D<float>  gPassWrite3D_F1[] : register( u5, space2 );
public RWByteAddressBuffer gPassWriteBuffer[] : register( u6, space2 );

// ============================================================================
// SPACE 3: PUSH CONSTANTS (Max 128 Bytes)
// ============================================================================
public struct PushConstant {
    public uint baseInstanceID; // Software Instance ID (Usually set by the renderer)
};

PUSH_CONSTANT( PushConstant, gPush, b0, space3 );
