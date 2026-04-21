// Vis.slang
#include "MaterialCommon.slang"

struct VSInput {
    uint vertexID   : SV_VertexID;
    uint instanceID : SV_InstanceID;
};
struct VSOutput {
    float4 position : SV_Position;
    nointerpolation uint instanceID : INSTANCE_ID;
    //For velocity computation
    float4 currentPos   : CURR_POS;
    float4 prevPos      : PREV_POS;
};

struct PSOutput {
    uint2  visibility   : SV_Target0;  // RT0: R32G32_UINT (VisBuffer)
    float2 velocity     : SV_Target1;  // RT1: R16G16_FLOAT (Velocity)
};

[shader( "vertex" )]
VSOutput vsVis( VSInput input ) {
    InstanceData instance = getInstanceData( input.instanceID );
    MeshData     mesh     = getMeshData( instance.meshID );

    // Vertex fetch (Vertex Pulling)
    CanonVertex v        = CanonVertex.fromIndex( input.vertexID, mesh.vertexOffset );
    float4      localPos = float4( v.position, 1.0 );

    float4 worldPos = mul( instance.modelMatrix, localPos );
    float4 clipPos  = mul( gFrame.viewProj, worldPos );

    VSOutput output;
    output.position   = clipPos;
    output.instanceID = input.instanceID;
    output.currentPos = clipPos;

    // Velocity Compute 
    // float4 prevWorldPos = mul(instance.prevModelMatrix, localPos);
    // output.prevPos = mul(gFrame.prevViewProj, prevWorldPos);
    // TBD
    output.prevPos = clipPos;

    return output;
}
[shader( "fragment" )]
PSOutput psVis( VSOutput input, uint primitiveID: SV_PrimitiveID ) {
    PSOutput output;

    // 1. VISIBILITY BUFFER (Target 0)
    output.visibility.x = primitiveID;
    output.visibility.y = input.instanceID;

    // 2. VELOCITY BUFFER (Target 1)
    float2 currentNDC = input.currentPos.xy / input.currentPos.w;
    float2 prevNDC    = input.prevPos.xy    / input.prevPos.w;
    // TODO: Check if I need to normalize these
    output.velocity = currentNDC - prevNDC;

    return output;
}
