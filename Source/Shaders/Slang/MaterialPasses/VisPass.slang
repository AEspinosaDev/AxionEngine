
#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;

// One mesh shading group per meshlet: enough threads for all its vertices and triangles without looping
static const uint MS_GROUP_SIZE = 128; // >= max( MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES )
static const uint AS_GROUP_SIZE = 32;  // Meshlets tested per amplification group, one wave (must match Config::VIS_AMPLIFICATION_GROUP_SIZE)

// -----------------------------------------------------------
// PASS I/O (Space 2): instances to draw, packed per command (GPU culled list, or the full sorted list without culling)
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gDrawInstances, 0 );

struct VisVertex {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    // For velocity computation
    float4 currentPos : CURR_POS;
    float4 prevPos : PREV_POS;
};

// Per primitive attributes: the triangle id comes from the mesh shader, not from SV_PrimitiveID (~2.6x slower on NVIDIA here)
struct VisPrimitive {
    nointerpolation uint triangleID : TRIANGLE_ID; // ( meshlet << VIS_TRIANGLE_BITS ) | local triangle
    nointerpolation uint instanceID : INSTANCE_ID; // Offset by 1 (0 = background)
    nointerpolation uint materialID : DATA0;
};

struct PSOutput {
    uint2  visibility : SV_Target0; // RT0: R32G32_UINT (VisBuffer)
    float2 velocity : SV_Target1;   // RT1: R16G16_FLOAT (Velocity)
};

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
    uint         meshletIndex = payload.meshletIndices[groupID.x];
    uint         instanceID   = payload.instanceID;
    InstanceData instance     = getInstanceData( instanceID );
    MeshData     mesh         = getMeshData( instance.meshID );
    Meshlet      meshlet      = getMeshlet( mesh, meshletIndex );

    SetMeshOutputCounts( meshlet.vertexCount, meshlet.triangleCount );

    // Vertices (vertex pulling through the meshlet vertex list)
    if ( groupThreadID < meshlet.vertexCount )
    {
        CanonVertex v        = CanonVertex.fromIndex( getMeshletVertexIndex( mesh, meshlet, groupThreadID ), mesh.vertexOffset );
        float4      worldPos = mul( instance.modelMatrix, float4( v.position, 1.0 ) );

        // WPO
        T material;
        material.init( instance.materialID );
        SurfaceData surface;
        surface.worldPos = worldPos.xyz;
        surface.shNormal = normalize( mul( (float3x3)instance.modelMatrix, v.normal ) );
        surface.uv       = v.uv;
        worldPos.xyz += material.computeWPO( surface );

        float4 clipPos = mul( gFrame.viewProj, worldPos );

        VisVertex output;
        output.position      = clipPos;
        output.uv            = v.uv;
        output.currentPos    = clipPos;
        output.prevPos       = clipPos; // Velocity TBD: prevModelMatrix / prevViewProj
        verts[groupThreadID] = output;
    }

    // Triangles
    if ( groupThreadID < meshlet.triangleCount )
    {
        tris[groupThreadID] = getMeshletTriangle( mesh, meshlet, groupThreadID );

        VisPrimitive prim;
        prim.triangleID      = packVisibilityTriangle( meshletIndex, groupThreadID );
        prim.instanceID      = instanceID + 1;
        prim.materialID      = instance.materialID;
        prims[groupThreadID] = prim;
    }
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "fragment" )]
PSOutput psVis<T : IMaterial>( VisVertex input, VisPrimitive prim ) {
    PSOutput output;

    // Alpha clipping
    T material;
    material.init( prim.materialID );

    SurfaceData surface;
    surface.uv = input.uv;
    if ( material.evalOpacity( surface ) < 0.5 )
    {
        discard;
    }

    // 1. VISIBILITY BUFFER (Target 0)
    output.visibility = uint2( prim.triangleID, prim.instanceID );

    // 2. VELOCITY BUFFER (Target 1)
    float2 currentNDC = input.currentPos.xy / input.currentPos.w;
    float2 prevNDC    = input.prevPos.xy / input.prevPos.w;
    // TODO: Check if I need to normalize these
    output.velocity = currentNDC - prevNDC;

    return output;
}
