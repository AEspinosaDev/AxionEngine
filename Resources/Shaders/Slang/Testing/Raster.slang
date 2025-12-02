struct VSInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

// --- UNIFORM BUFFER ---
struct CameraData {
    float4x4 viewProj;
};
cbuffer CameraCB : register( b0, space0 ) {
    CameraData gCam;
};

[shader( "vertex" )]
VSOutput vsMain( VSInput input ) {
    VSOutput output;

    output.position = mul( gCam.viewProj, float4( input.position, 1.0 ) );
    output.color    = input.color;

    return output;
}

[shader( "fragment" )]
float4 psMain( VSOutput input )
    : SV_Target {
    return float4( input.color, 1.0 );
}
