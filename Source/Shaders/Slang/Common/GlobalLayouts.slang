#include "Defines.slang"
#include "Scene.slang"

// ============================================================================
// GLOBAL LAYOUT CONTRACT (must match RasterizerConfig.h / RasterizerConfig.cpp)
//   space0 : Persistent global buffers   (descriptor table)
//   space1 : Frame data                  (descriptor table)
//   space2 : Pass I/O                    (declared by each pass shader)
//   space3 : Push constants              (declared by each pass / material shader)
//
//   Textures & samplers are pure bindless: accessed by index into the
//   global shader-visible heaps (SM 6.6 ResourceDescriptorHeap).
// ============================================================================

#define SPACE_PERSISTENT space0
#define SPACE_FRAME      space1
#define SPACE_PASS       space2
#define SPACE_PUSH       space3

// Push constant register (use with PUSH_CONSTANT( Type, Name, PUSH_CONSTANT_REG, SPACE_PUSH ))
#define PUSH_CONSTANT_REG b0

// Space 2 slot registers (REG_PASS_* in RasterizerConfig.h)
// Usage: Texture2D<uint2> gVisBuffer : register( PASS_SRV_IMAGE_0, SPACE_PASS );
#define PASS_SRV_IMAGE_0 t0
#define PASS_SRV_IMAGE_1 t1
#define PASS_SRV_IMAGE_2 t2
#define PASS_SRV_IMAGE_3 t3
#define PASS_SRV_IMAGE_4 t4
#define PASS_SRV_IMAGE_5 t5
#define PASS_SRV_IMAGE_6 t6
#define PASS_SRV_IMAGE_7 t7

#define PASS_SRV_BUFFER_0 t8
#define PASS_SRV_BUFFER_1 t9
#define PASS_SRV_BUFFER_2 t10
#define PASS_SRV_BUFFER_3 t11
#define PASS_SRV_BUFFER_4 t12
#define PASS_SRV_BUFFER_5 t13
#define PASS_SRV_BUFFER_6 t14
#define PASS_SRV_BUFFER_7 t15

#define PASS_UAV_IMAGE_0 u0
#define PASS_UAV_IMAGE_1 u1
#define PASS_UAV_IMAGE_2 u2
#define PASS_UAV_IMAGE_3 u3
#define PASS_UAV_IMAGE_4 u4
#define PASS_UAV_IMAGE_5 u5
#define PASS_UAV_IMAGE_6 u6
#define PASS_UAV_IMAGE_7 u7

#define PASS_UAV_BUFFER_0 u8
#define PASS_UAV_BUFFER_1 u9
#define PASS_UAV_BUFFER_2 u10
#define PASS_UAV_BUFFER_3 u11
#define PASS_UAV_BUFFER_4 u12
#define PASS_UAV_BUFFER_5 u13
#define PASS_UAV_BUFFER_6 u14
#define PASS_UAV_BUFFER_7 u15

// ============================================================================
// SPACE 0: PERSISTENT GLOBAL BUFFERS
// ============================================================================
public ByteAddressBuffer gGlobalVertices  : register( t0, SPACE_PERSISTENT );
public ByteAddressBuffer gGlobalIndices   : register( t1, SPACE_PERSISTENT );
public ByteAddressBuffer gGlobalMaterials : register( t2, SPACE_PERSISTENT );

// ============================================================================
// SPACE 1: FRAME DATA (Updated Per-Frame)
// ============================================================================
public ConstantBuffer<FrameData>      gFrame               : register( b0, SPACE_FRAME );
public StructuredBuffer<MeshData>     gMeshes              : register( t0, SPACE_FRAME );
public StructuredBuffer<MaterialData> gMaterials           : register( t1, SPACE_FRAME );
public StructuredBuffer<InstanceData> gInstances           : register( t2, SPACE_FRAME );
public StructuredBuffer<LightData>    gLights              : register( t3, SPACE_FRAME );
public StructuredBuffer<EnvData>      gEnvs                : register( t4, SPACE_FRAME );
public StructuredBuffer<uint>         gInstanceRedirection : register( t5, SPACE_FRAME );

// ============================================================================
// BINDLESS ACCESS (Global Heap)
// ============================================================================
public Texture2D getTexture2D( uint idx ) {
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex( idx )];
    return tex;
}
public Texture3D getTexture3D( uint idx ) {
    Texture3D tex = ResourceDescriptorHeap[NonUniformResourceIndex( idx )];
    return tex;
}
public TextureCube getTextureCube( uint idx ) {
    TextureCube tex = ResourceDescriptorHeap[NonUniformResourceIndex( idx )];
    return tex;
}
public SamplerState getSampler( uint idx ) {
    SamplerState samp = SamplerDescriptorHeap[NonUniformResourceIndex( idx )];
    return samp;
}
