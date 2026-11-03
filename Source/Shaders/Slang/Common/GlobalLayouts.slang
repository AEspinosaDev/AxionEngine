// MODULE: engine global shader parameters (space 0 & 1). import it (usually through MaterialCommon).
#include "Defines.slang"   // Register / space macros
__exported import Scene;    // Types of space 1 buffers
__exported import Bindless; // Textures & samplers access

// ============================================================================
// GLOBAL LAYOUT CONTRACT (must match RasterizerConfig.h / RasterizerConfig.cpp)
//   space0 : Persistent global buffers   (descriptor table)
//   space1 : Frame data                  (descriptor table)
//   space2 : Pass I/O                    (declared by each pass shader, DECLARE_* macros in Defines.slang)
//   space3 : Push constants              (declared by each pass / material shader)
//
//   Textures & samplers are pure bindless: accessed by index into the
//   global shader-visible heaps (Bindless.slang).
// ============================================================================

// ============================================================================
// SPACE 0: PERSISTENT GLOBAL BUFFERS
// ============================================================================
public ByteAddressBuffer gGlobalVertices          : register( t0, SPACE_PERSISTENT );
public ByteAddressBuffer gGlobalIndices           : register( t1, SPACE_PERSISTENT );
public ByteAddressBuffer gGlobalMeshlets          : register( t2, SPACE_PERSISTENT ); // Meshlet headers (48 B)
public ByteAddressBuffer gGlobalMeshletVertices   : register( t3, SPACE_PERSISTENT ); // u32 -> mesh local vertex index
public ByteAddressBuffer gGlobalMeshletPrimitives : register( t4, SPACE_PERSISTENT ); // u8 x3 -> meshlet local triangle
public ByteAddressBuffer gGlobalMaterials         : register( t5, SPACE_PERSISTENT );

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
public StructuredBuffer<TextureData>  gTextures            : register( t6, SPACE_FRAME );
