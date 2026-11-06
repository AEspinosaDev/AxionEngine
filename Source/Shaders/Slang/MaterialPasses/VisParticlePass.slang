#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;
import VisObject;
#include "Vis.slang" // Vertex, primitive and pixel shader shared with the instances

// -----------------------------------------------------------
// PASS I/O (Space 1). The particles and the visible list of each system are bindless
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gVisibleCounts, 0 ); // Visible particles per system

// Indirect contract: systemIndex must be the first field (written by the command signature). Grid = ( meshlets, Y, Z )
struct VisParticlePushConstant {
    uint systemIndex;
};
DECLARE_PUSH_CONSTANT( VisParticlePushConstant, gPush );

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL ! No amplification: the particles were culled by their update
[shader( "mesh" )]
[outputtopology( "triangle" )]
[numthreads( MS_GROUP_SIZE, 1, 1 )]
void msVisParticle<T : IMaterial>( uint                                                  groupThreadID: SV_GroupThreadID,
                                   uint3                                                 groupID: SV_GroupID,
                                   OutputVertices<VisVertex, MESHLET_MAX_VERTICES>       verts,
                                   OutputIndices<uint3, MESHLET_MAX_TRIANGLES>           tris,
                                   OutputPrimitives<VisPrimitive, MESHLET_MAX_TRIANGLES> prims ) {
    ParticleSystemData system       = gParticleSystems[gPush.systemIndex];
    MeshData           mesh         = getMeshData( system.meshID );
    const uint         meshletIndex = groupID.x;
    Meshlet            meshlet      = getMeshlet( mesh, meshletIndex );

    // Grid -> visible list. The last row can have more groups than particles: those output nothing
    const uint visibleCount = gVisibleCounts[gPush.systemIndex];
    const uint gridY        = getParticleDispatchGrid( visibleCount, mesh.meshletCount ).y;
    const uint visibleIndex = groupID.y + groupID.z * gridY;
    const bool hasParticle  = visibleIndex < visibleCount;

    const uint vertexCount   = hasParticle ? meshlet.vertexCount : 0;
    const uint triangleCount = hasParticle ? meshlet.triangleCount : 0;
    SetMeshOutputCounts( vertexCount, triangleCount );

    if ( groupThreadID >= max( vertexCount, triangleCount ) )
        return;

    StructuredBuffer<uint> visibleList   = getStructuredBuffer<uint>( system.visibleListSRVHeapID );
    const uint             particleIndex = visibleList[visibleIndex];

    // Vertices
    if ( groupThreadID < vertexCount )
        verts[groupThreadID] = buildVisVertex<T>( VisParticle( gPush.systemIndex, particleIndex ), mesh, meshlet, groupThreadID );

    // Triangles
    if ( groupThreadID < triangleCount )
    {
        tris[groupThreadID]  = getMeshletTriangle( mesh, meshlet, groupThreadID );
        prims[groupThreadID] = buildVisPrimitive( meshletIndex, groupThreadID, packVisibilityParticle( gPush.systemIndex, particleIndex ), system.materialID );
    }
}
