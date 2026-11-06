
#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;
import VisObject;
#include "Vis.slang" // Vertex, primitive and pixel shader shared with the particles

static const uint AS_GROUP_SIZE = 32; // Meshlets tested per amplification group, one wave (must match Config::VIS_AMPLIFICATION_GROUP_SIZE)

// -----------------------------------------------------------
// PASS I/O (Space 1): instances to draw, packed per command (GPU culled list, or the full sorted list without culling)
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gDrawInstances, 0 );

// Amplification -> mesh: instance + its visible meshlets (compacted)
struct VisPayload {
    uint instanceID;
    uint meshletIndices[AS_GROUP_SIZE];
};
groupshared VisPayload gsPayload;

// Indirect contract: baseInstanceID must be the first field (written by the command signature). Grid = ( meshlet chunks, instances, 1 )
struct VisPushConstant {
    uint baseInstanceID;
    uint meshletConeCulling; ///< Per batch (CPU): 1 if the PSO culls back faces
};
DECLARE_PUSH_CONSTANT( VisPushConstant, gPush );

bool isMeshletVisible( Meshlet meshlet, InstanceData instance ) {
    float3 center = mul( instance.modelMatrix, float4( meshlet.center, 1.0 ) ).xyz;
    if ( !isSphereInFrustum( center, meshlet.radius * getMaxScale( instance.modelMatrix ) ) )
        return false;

    if ( gPush.meshletConeCulling != 0 )
    {
        float4 cone = unpackMeshletCone( meshlet.packedCone );
        float3 apex = mul( instance.modelMatrix, float4( meshlet.coneApex, 1.0 ) ).xyz;
        float3 axis = normalize( mul( (float3x3)instance.modelMatrix, cone.xyz ) ); // Uniform scale assumed (as the resolve normals)
        if ( isConeBackfacing( apex, axis, cone.w, gFrame.getCameraPos() ) )
            return false;
    }
    return true;
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL ! (generic only to share the pass specialization)
[shader( "amplification" )]
[numthreads( AS_GROUP_SIZE, 1, 1 )]
void asVis<T : IMaterial>( uint groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID ) {
    uint         instanceID   = gDrawInstances[gPush.baseInstanceID + groupID.y];
    InstanceData instance     = getInstanceData( instanceID );
    MeshData     mesh         = getMeshData( instance.meshID );
    uint         meshletIndex = groupID.x * AS_GROUP_SIZE + groupThreadID;

    bool visible = meshletIndex < mesh.meshletCount && isMeshletVisible( getMeshlet( mesh, meshletIndex ), instance );

    // Compact the visible meshlets (the group is a single wave)
    uint slot = WavePrefixCountBits( visible );
    if ( visible )
        gsPayload.meshletIndices[slot] = meshletIndex;
    if ( groupThreadID == 0 )
        gsPayload.instanceID = instanceID;

    DispatchMesh( WaveActiveCountBits( visible ), 1, 1, gsPayload );
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "mesh" )]
[outputtopology( "triangle" )]
[numthreads( MS_GROUP_SIZE, 1, 1 )]
void msVis<T : IMaterial>( uint                                                  groupThreadID: SV_GroupThreadID,
                           uint3                                                 groupID: SV_GroupID,
                           in payload VisPayload                                 payload,
                           OutputVertices<VisVertex, MESHLET_MAX_VERTICES>       verts,
                           OutputIndices<uint3, MESHLET_MAX_TRIANGLES>           tris,
                           OutputPrimitives<VisPrimitive, MESHLET_MAX_TRIANGLES> prims ) {
    uint        meshletIndex = payload.meshletIndices[groupID.x];
    uint        instanceID   = payload.instanceID;
    VisInstance object       = VisInstance( instanceID );
    MeshData    mesh         = getMeshData( object.getMeshID() );
    Meshlet     meshlet      = getMeshlet( mesh, meshletIndex );

    SetMeshOutputCounts( meshlet.vertexCount, meshlet.triangleCount );

    // Vertices
    if ( groupThreadID < meshlet.vertexCount )
        verts[groupThreadID] = buildVisVertex<T>( object, mesh, meshlet, groupThreadID );

    // Triangles
    if ( groupThreadID < meshlet.triangleCount )
    {
        tris[groupThreadID]  = getMeshletTriangle( mesh, meshlet, groupThreadID );
        prims[groupThreadID] = buildVisPrimitive( meshletIndex, groupThreadID, instanceID + 1, object.getMaterialID() );
    }
}
