// MODULE: engine global shader parameters (space 0). import it (usually through MaterialCommon).
#include "Defines.slang"   // Register / space macros
__exported import Scene;    // Types of space 0 buffers
__exported import Bindless; // Textures & samplers access

// ============================================================================
// GLOBAL LAYOUT CONTRACT (must match RasterizerConfig.h / RasterizerConfig.cpp)
//   space0 : Frame data                  (descriptor table)
//   space1 : Pass I/O                    (declared by each pass shader, DECLARE_* macros in Defines.slang)
//   space2 : Push constants              (declared by each pass / material shader)
//
//   Persistent buffers (geometry pages, materials), textures & samplers are pure bindless:
//   accessed by index into the global shader-visible heaps (Bindless.slang). Heap IDs live in
//   MeshData (geometry page) and FrameData (materials, defaults).
// ============================================================================

// ============================================================================
// SPACE 0: FRAME DATA (Updated Per-Frame)
// ============================================================================
public ConstantBuffer<FrameData>      gFrame               : register( b0, SPACE_FRAME );
public StructuredBuffer<MeshData>     gMeshes              : register( t0, SPACE_FRAME );
public StructuredBuffer<MaterialData> gMaterials           : register( t1, SPACE_FRAME );
public StructuredBuffer<InstanceData> gInstances           : register( t2, SPACE_FRAME );
public StructuredBuffer<LightData>    gLights              : register( t3, SPACE_FRAME );
public StructuredBuffer<EnvData>      gEnvs                : register( t4, SPACE_FRAME );
public StructuredBuffer<uint>         gInstanceRedirection : register( t5, SPACE_FRAME );
public StructuredBuffer<TextureData>  gTextures            : register( t6, SPACE_FRAME );
