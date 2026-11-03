#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
#include "Tonemappers.slang"

DECLARE_TEXTURE( Texture2D<float4>, gInput, 0 );
DECLARE_RW_TEXTURE( RWTexture2D<float4>, gOutput, 0 );

public struct PushConstant {
    public float exposure;
};

DECLARE_PUSH_CONSTANT( PushConstant, gPush );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void computeMain( uint3 dispatchThreadId: SV_DispatchThreadID ) {
    uint width, height;
    gOutput.GetDimensions( width, height );

    uint2 pixelCoord = dispatchThreadId.xy;
    if ( pixelCoord.x >= width || pixelCoord.y >= height )
        return;

    float4 rawData  = gInput.Load( int3( pixelCoord, 0 ) );
    float3 hdrColor = rawData.rgb * gPush.exposure;

    float3 ldrLinear = ACESFilm( hdrColor );

    float3 ldrSRGB = pow( ldrLinear, 1.0 / 2.2 );

    gOutput[pixelCoord] = float4( ldrSRGB, 1.0 );
}
