
#include "MaterialCommon.slang"
#include "Materials/IMaterial.slang"

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};
struct VSOutput {
    float4               position : SV_Position;
    float2               uv : TEXCOORD0;
    nointerpolation uint instanceID : INSTANCE_ID;
    nointerpolation uint materialID : DATA0;
    // For velocity computation
    float4 currentPos : CURR_POS;
    float4 prevPos : PREV_POS;
};

struct PSOutput {
    uint2  visibility : SV_Target0; // RT0: R32G32_UINT (VisBuffer)
    float2 velocity : SV_Target1;   // RT1: R16G16_FLOAT (Velocity)
};

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "vertex" )]
VSOutput vsVis<T : IMaterial>( VSInput input ) {
    uint         instanceID = getRedirectedInstanceID( input.instanceID );
    InstanceData instance   = getInstanceData( instanceID );
    MeshData     mesh       = getMeshData( instance.meshID );

    // Vertex fetch (Vertex Pulling)
    CanonVertex v        = CanonVertex.fromIndex( input.vertexID, mesh.vertexOffset );
    float4      localPos = float4( v.position, 1.0 );
    float3      localNrm = v.normal;
    float2      uv       = v.uv;

    float4 worldPos = mul( instance.modelMatrix, localPos );
    float3 worldNrm = normalize( mul( (float3x3)instance.modelMatrix, localNrm ) );

    // WPO
    T material;
    material.init( instance.materialID );
    SurfaceData surface;
    surface.worldPos = worldPos.xyz;
    surface.shNormal = worldNrm;
    surface.uv       = uv;
    worldPos.xyz += material.computeWPO( surface );

    float4 clipPos = mul( gFrame.viewProj, worldPos );

    VSOutput output;
    output.position   = clipPos;
    output.instanceID = instanceID + 1;
    output.materialID = instance.materialID;
    output.currentPos = clipPos;
    output.uv         = uv;

    // Velocity Compute
    // float4 prevWorldPos = mul(instance.prevModelMatrix, localPos);
    // output.prevPos = mul(gFrame.prevViewProj, prevWorldPos);
    // TBD

    output.prevPos = clipPos;

    return output;
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "fragment" )]
PSOutput psVis<T : IMaterial>( VSOutput input, uint primitiveID: SV_PrimitiveID ) {
    PSOutput output;

    // Alpha clipping
    T material;
    material.init( input.materialID );

    SurfaceData surface;
    surface.uv = input.uv;
    if ( material.evalOpacity( surface ) < 0.5 )
    {
        discard;
    }

    // 1. VISIBILITY BUFFER (Target 0)
    output.visibility.x = primitiveID;
    output.visibility.y = input.instanceID;

    // 2. VELOCITY BUFFER (Target 1)
    float2 currentNDC = input.currentPos.xy / input.currentPos.w;
    float2 prevNDC    = input.prevPos.xy / input.prevPos.w;
    // TODO: Check if I need to normalize these
    output.velocity = currentNDC - prevNDC;

    return output;
}
