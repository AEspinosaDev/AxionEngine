#include "Tonemappers.slang"

Texture2D<float4> gInput : register(t0, space0); 
RWTexture2D<float4> gOutput : register(u0, space1);


[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width, height;
    gOutput.GetDimensions(width, height); 

    uint2 pixelCoord = dispatchThreadId.xy;
    if (pixelCoord.x >= width || pixelCoord.y >= height) 
        return;

    float4 hdrColor = gInput.Load(int3(pixelCoord, 0));

    float3 ldrColor = ACESFilm(hdrColor.rgb);

    ldrColor = pow(ldrColor, 1.0 / 2.2);

    gOutput[pixelCoord] = float4(ldrColor, 1.0);
}