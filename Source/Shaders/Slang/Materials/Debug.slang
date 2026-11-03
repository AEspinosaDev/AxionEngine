// Test.slang / Unlit.slang
#include "MaterialCommon.slang"

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// -----------------------------------------------------------
// SHADERS
// -----------------------------------------------------------

[shader( "vertex" )]
VSOutput vsForward( VSInput input ) {
    InstanceData instance = getInstanceData(input.instanceID);
    MeshData     mesh     = getMeshData( instance.meshID );

    // Vertex fetch
    CanonVertex v        = CanonVertex.fromIndex( input.vertexID, mesh.vertexOffset );
    float3      localPos = v.position;
    float2      uv       = v.uv;

    VSOutput output;
    float4   worldPos = mul( instance.modelMatrix, float4( localPos, 1.0 ) );
    output.position   = mul( gFrame.viewProj, worldPos );
    output.uv         = uv;

    return output;
}

[shader( "fragment" )]
float4 psForward( VSOutput input )
    : SV_Target {
    return float4( 1.0, 0.0, 0.5, 1.0 );
}
