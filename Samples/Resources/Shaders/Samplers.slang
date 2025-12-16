struct VSInput {
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct CameraData {
    float4x4 viewModelProj;
};
cbuffer CameraCB : register(b0, space0) {
    CameraData gCam;
};

Texture2D<float4> gTexture : register(t1, space0);
SamplerState gSampler : register(s0, space0);

[shader("vertex")]
VSOutput vsMain(VSInput input) {
    VSOutput output;

    output.position = mul( gCam.viewModelProj, float4(input.position, 1.0));
    output.uv = input.uv;

    return output;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target {

    float4 color = gTexture.Sample( gSampler, input.uv );
    float  gamma = 2.2;
    return pow(color, gamma);
}