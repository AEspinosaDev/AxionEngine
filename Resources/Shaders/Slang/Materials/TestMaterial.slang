

#include "Defines.slang"
#include "Scene.slang"

struct VSInput {
    uint vertexID : SV_VertexID; // Vertex natural iterator
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

// Persistent
ByteAddressBuffer gGlobalVertices : register( t0, space0 );
ByteAddressBuffer gGlobalIndices : register( t1, space0 );

// Transient
ConstantBuffer<FrameData>      gFrame : register( b0, space1 );
StructuredBuffer<MeshData>     gMeshes : register( t0, space1 );
StructuredBuffer<InstanceData> gInstances : register( t1, space1 );
StructuredBuffer<LightData>    gLights : register( t2, space1 );

// Entry Point PC
struct PushConstant {
    uint instanceID;
};
PUSH_CONSTANT( PushConstant, gPush, b0, space2 );

// FORWARD PASS
[shader( "vertex" )]
VSOutput vsForward( VSInput input ) {

    InstanceData instance = gInstances[gPush.instanceID];
    MeshData     mesh     = gMeshes[instance.meshID];

    // Index Pulling
    uint indexAddress = mesh.indexOffset + ( input.vertexID * 4 );
    uint vertexIndex  = gGlobalIndices.Load( indexAddress );

    // Vertex Pulling
    uint vertexAddress = mesh.vertexOffset + ( vertexIndex * CANON_VERTEX_STRIDE );

    // Read Vertices
    float3 localPos = asfloat( gGlobalVertices.Load3( vertexAddress ) );      // Offset 0
    float3 localNrm = asfloat( gGlobalVertices.Load3( vertexAddress + 12 ) ); // Offset 12
    float2 uv       = asfloat( gGlobalVertices.Load2( vertexAddress + 24 ) ); // Offset 24

    VSOutput output;
    float4   worldPos = mul( instance.modelMatrix, float4( localPos, 1.0 ) );
    // float4   worldPos =  float4( localPos, 1.0 ) ;
    output.position   = mul( gFrame.viewProj, worldPos );
    output.normal     = normalize( mul( (float3x3)instance.normalMatrix, localNrm ) );
    output.uv         = uv;

    return output;
}

[shader( "fragment" )]
float4 psForward( VSOutput input )
    : SV_Target {

    return float4( 1.0, 1.0, 1.0, 1.0 );
}
