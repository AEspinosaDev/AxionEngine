#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;   // gFrame, gMeshes, gInstances, gInstanceRedirection (full sorted list)
import IndirectCommand;

static const uint  CULLING_GROUP_SIZE    = 64;  // Must match CullingPass::GROUP_SIZE
static const float CULLING_RADIUS_MARGIN = 1.2; // Conservative bounding sphere

// -----------------------------------------------------------
// PASS I/O (Space 2)
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gCommandMap, 0 );                            // Sorted instance -> indirect command
DECLARE_RW_BUFFER( RWStructuredBuffer<DispatchMeshIndirectCommand>, gCommands, 0 ); // threadGroupCountY: visible instances
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gVisibleInstances, 1 );                // Compacted per command (from its baseInstanceID)

struct CullingPushConstant {
    uint instanceCount;
};
DECLARE_PUSH_CONSTANT( CullingPushConstant, gPush );

[shader( "compute" )]
[numthreads( CULLING_GROUP_SIZE, 1, 1 )]
void csCulling( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    uint sortedIdx = dispatchThreadID.x;
    if ( sortedIdx >= gPush.instanceCount )
        return;

    uint         instanceID = gInstanceRedirection[sortedIdx];
    InstanceData instance   = getInstanceData( instanceID );
    if ( instance.active == 0 )
        return;

    // Frustum vs world bounding sphere
    MeshData mesh        = getMeshData( instance.meshID );
    float3   worldCenter = mul( instance.modelMatrix, float4( mesh.bsphere.xyz, 1.0 ) ).xyz;
    float    worldRadius = mesh.bsphere.w * getMaxScale( instance.modelMatrix ) * CULLING_RADIUS_MARGIN;
    if ( !isSphereInFrustum( worldCenter, worldRadius ) )
        return;

    // Append to its command
    uint cmd = gCommandMap[sortedIdx];
    uint slot;
    InterlockedAdd( gCommands[cmd].threadGroupCountY, 1, slot );
    gVisibleInstances[gCommands[cmd].baseInstanceID + slot] = instanceID;
}
