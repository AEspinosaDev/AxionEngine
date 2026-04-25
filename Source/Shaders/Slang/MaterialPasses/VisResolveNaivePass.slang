#include "MaterialCommon.slang"
#include "Materials/IMaterial.slang"

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void csResolve<T : IMaterial>( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    uint2 pixelCoord = dispatchThreadID.xy;

    // Evitar leer fuera de la pantalla si la resolución no es múltiplo de 8
    if ( any( pixelCoord >= gFrame.getResolution() ) )
        return;

    uint2 visData     = gPassRead2D_U2[0].Load( int3( pixelCoord, 0 ) );
    uint  primitiveID = visData.x;
    uint  instanceID  = visData.y;

    // Instance offseted by
    if ( instanceID == 0 )
    {
        gPassWrite2D_F4[0][pixelCoord] = float4( 0.1, 0.1, 0.2, 1.0 );
        return;
    }

    InstanceData instance = getInstanceData( instanceID - 1u );
    MeshData     mesh     = getMeshData( instance.meshID );

    uint indexStride     = 4; // Bytes
    uint baseIndexOffset = mesh.indexOffset + ( primitiveID * 3 * indexStride );

    uint i0 = gGlobalIndices.Load( baseIndexOffset );
    uint i1 = gGlobalIndices.Load( baseIndexOffset + 4 );
    uint i2 = gGlobalIndices.Load( baseIndexOffset + 8 );

    CanonVertex v0 = CanonVertex.fromIndex( i0, mesh.vertexOffset );
    CanonVertex v1 = CanonVertex.fromIndex( i1, mesh.vertexOffset );
    CanonVertex v2 = CanonVertex.fromIndex( i2, mesh.vertexOffset );

    // Barycentrics
    float3 barycentrics = calculateBarycentrics( pixelCoord, v0.position, v1.position, v2.position, instance.modelMatrix, gFrame.viewProj, gFrame.getResolution() );
    
    // Interpolation
    float3 localPos = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
    float3 localNrm = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    float2 uv       = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;

    SurfaceData surface;
    surface.worldPos = mul( instance.modelMatrix, float4( localPos, 1.0 ) ).xyz;
    surface.shNormal = normalize( mul( (float3x3)instance.modelMatrix, localNrm ) );
    surface.uv       = uv;
    surface.V        = normalize( gFrame.getCameraPos() - surface.worldPos );

    T material;
    material.init( instance.materialID );

    gPassWrite2D_F4[0][pixelCoord] = float4( material.evalColor( surface ), 1.0 );

}
