RWTexture2D<float4> gOutput : register( u0, space0 );

struct Data {
    float time;
    float speed;
};


cbuffer PushConstants {
    Data gConst;
};

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void computeMain( uint3 dispatchThreadId: SV_DispatchThreadID ) {
    uint width, height;
    gOutput.GetDimensions( width, height );

    uint2 pixelCoord = dispatchThreadId.xy;

    if ( pixelCoord.x >= width || pixelCoord.y >= height )
        return;

    float2 uv = float2( pixelCoord ) / float2( width, height );

    float r = 0.5 + 0.5 * sin( gConst.time * gConst.speed + uv.x * 4.0 );
    float g = 0.5 + 0.5 * cos( gConst.time * gConst.speed + uv.y * 4.0 );
    float b = 0.5 + 0.5 * sin( gConst.time * gConst.speed * 0.5 ); // Cambia lento globalmente

    float3 color = float3( r, g, b ); // Color base LDR

    float dist = distance( uv, float2( 0.5, 0.5 ) );

    float radius = 0.25 + 0.1 * sin( gConst.time * gConst.speed * 3.0 );

    float circle = 1.0 - smoothstep( radius, radius + 0.05, dist );

    float3 circleColor = 1.0;
    circleColor        = abs( circleColor ) * 10.0;

    color += circleColor * circle;

    // Escribir
    gOutput[pixelCoord] = float4( color, 1.0 );
}
