#include "Defines.slang"

struct VSInput {
    uint vertexID : SV_VertexID;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct CameraData {
    float4x4 viewProj;
};

struct Vertex {
    float3 pos;
    float3 normal;
    float2 uv;
};

ByteAddressBuffer gVertices : register( t0, space0 );
ByteAddressBuffer gIndices : register( t1, space0 );
cbuffer           CameraCB : register( b0, space0 ) {
    CameraData gCam;
};

struct Data {
    float4x4 model;
    float4   color;
    uint     meshOffset;
    uint     meshIdOffset;
};

PUSH_CONSTANT( Data, gPush, b0, space1 );

static const uint VERTEX_STRIDE = 32;

[shader( "vertex" )]
VSOutput vsMain( VSInput input ) {
    VSOutput output;

    // One ID (uint) is 4 bytes
    uint indexAddress = gPush.meshIdOffset +( input.vertexID * 4);
    uint vertexIndex  = gIndices.Load( indexAddress );

    uint   vertexAddress = gPush.meshOffset + (vertexIndex * VERTEX_STRIDE);
    float3 pos           = asfloat( gVertices.Load3( vertexAddress ) );
    float2 uv            = asfloat( gVertices.Load2( vertexAddress + 24 ) );

    float4 worldPos = mul( gPush.model, float4( pos, 1.0 ) );
    output.position = mul( gCam.viewProj, worldPos );

    output.uv = uv;

    return output;
}

[shader( "fragment" )]
float4 psMain( VSOutput input )
    : SV_Target {
    float gamma = 2.2;
    return pow( gPush.color, gamma );
}
