

#include "Common/MaterialCommon.slang"
#include "Materials/IMaterial.slang"

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4               position : SV_Position;
    float2               uv : TEXCOORD0;
    float3               normal : NORMAL;
    float3               worldPos : TEXCOORD1;
    nointerpolation uint materialID : DATA0;
};

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "vertex" )]
VSOutput vsForward<T : IMaterial>( VSInput input ) {
    uint         instanceID = getRedirectedInstanceID( input.instanceID );
    InstanceData instance   = getInstanceData( instanceID );
    MeshData     mesh       = getMeshData( instance.meshID );

    // Vertex fetch
    CanonVertex v        = CanonVertex.fromIndex( input.vertexID, mesh.vertexOffset );
    float3      localPos = v.position;
    float3      localNrm = v.normal;
    float2      uv       = v.uv;

    float4 worldPos = mul( instance.modelMatrix, float4( localPos, 1.0 ) );
    float3 worldNrm = normalize( mul( (float3x3)instance.modelMatrix, localNrm ) );

    // **********************
    // TBD: TBN Matrix
    // **********************

    // WPO
    T material;
    material.init( instance.materialID );
    SurfaceData surface;
    surface.worldPos = worldPos.xyz;
    surface.shNormal = worldNrm;
    surface.uv       = uv;
    worldPos.xyz += material.computeWPO( surface );

    VSOutput output;
    output.position   = mul( gFrame.viewProj, worldPos );
    output.worldPos   = worldPos.xyz;
    output.normal     = worldNrm;
    output.uv         = uv;
    output.materialID = instance.materialID;

    return output;
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "fragment" )]
float4 psForward<T : IMaterial>( VSOutput input )
    : SV_Target {

    SurfaceData surface;
    surface.worldPos = input.worldPos;
    surface.shNormal = input.normal;
    surface.uv       = input.uv;
    surface.V        = normalize( gFrame.getCameraPos() - surface.worldPos );

    T material;
    material.init( input.materialID );

    return float4( material.evalColor( surface ), 1.0 );
}
