// TAA resolve: HDR in, HDR out (the output is the history of the next frame).
// Working space: exposed + reversible tonemap + YCoCg. Current frame neighborhood -> variance box (Salvi) intersected with its min / max.
// History: reprojected with the dilated velocity, Catmull-Rom fetch, clipped towards the box center (Playdead).
#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import Bindless;

DECLARE_TEXTURE( Texture2D<float4>, gCurrImg,  0 ); ///< HDR, jittered
DECLARE_TEXTURE( Texture2D<float4>, gPrevImg,  1 ); ///< HDR, TAA output of the previous frame
DECLARE_TEXTURE( Texture2D<float2>, gVelocity, 2 ); ///< UV units: current - previous

DECLARE_RW_TEXTURE( RWTexture2D<float4>, gOutput, 0 );

public struct PushConstant {
    float2 inverseTexSize;
    uint   samplerID;    ///< Bindless linear CLAMP sampler (the Catmull-Rom taps reach outside the screen)
    float  exposure;     ///< Same factor as the tonemapper
    uint   resetHistory; ///< 1 -> output = current frame
};

DECLARE_PUSH_CONSTANT( PushConstant, gPush );

static const float BASE_ALPHA     = 0.1;  ///< Current frame weight when static (~10 frames of history)
static const float MOTION_ALPHA   = 0.25; ///< Current frame weight at MOTION_PIXELS or faster (less blur)
static const float MOTION_PIXELS  = 8.0;  ///< Speed (pixels / frame) that reaches MOTION_ALPHA
static const float VARIANCE_GAMMA = 1.0;  ///< Box half size in standard deviations

float luma( float3 c ) {
    return dot( c, float3( 0.2126, 0.7152, 0.0722 ) );
}

/// @brief Reversible tonemap (Karis): blending here weights every sample by 1 / (1 + luma), no flickering fireflies
float3 tonemap( float3 c ) {
    return c * rcp( 1.0 + luma( c ) );
}
float3 inverseTonemap( float3 c ) {
    return c * rcp( max( 1.0 - luma( c ), 1e-5 ) );
}

float3 rgbToYCoCg( float3 c ) {
    return float3( 0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                   0.5 * c.r - 0.5 * c.b,
                   -0.25 * c.r + 0.5 * c.g - 0.25 * c.b );
}
float3 yCoCgToRgb( float3 c ) {
    return float3( c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z );
}

/// @brief HDR -> working space (exposed, tonemapped, YCoCg)
float3 toWorking( float3 hdr ) {
    return rgbToYCoCg( tonemap( hdr * gPush.exposure ) );
}
/// @brief Working space -> HDR
float3 fromWorking( float3 w ) {
    return inverseTonemap( yCoCgToRgb( w ) ) / gPush.exposure;
}

/// @brief Catmull-Rom history fetch in 5 bilinear taps (the 4 corners are dropped, their weight is tiny)
float3 sampleHistoryCatmullRom( SamplerState linearClamp, float2 uv ) {
    const float2 texSize  = rcp( gPush.inverseTexSize );
    const float2 position = uv * texSize;
    const float2 center   = floor( position - 0.5 ) + 0.5;
    const float2 f        = position - center;

    const float2 w0  = f * ( -0.5 + f * ( 1.0 - 0.5 * f ) );
    const float2 w1  = 1.0 + f * f * ( -2.5 + 1.5 * f );
    const float2 w2  = f * ( 0.5 + f * ( 2.0 - 1.5 * f ) );
    const float2 w3  = f * f * ( -0.5 + 0.5 * f );
    const float2 w12 = w1 + w2;

    const float2 uv0  = ( center - 1.0 ) * gPush.inverseTexSize;
    const float2 uv3  = ( center + 2.0 ) * gPush.inverseTexSize;
    const float2 uv12 = ( center + w2 / w12 ) * gPush.inverseTexSize;

    float3 result = gPrevImg.SampleLevel( linearClamp, float2( uv12.x, uv0.y ), 0 ).rgb * ( w12.x * w0.y );
    result += gPrevImg.SampleLevel( linearClamp, float2( uv0.x, uv12.y ), 0 ).rgb * ( w0.x * w12.y );
    result += gPrevImg.SampleLevel( linearClamp, float2( uv12.x, uv12.y ), 0 ).rgb * ( w12.x * w12.y );
    result += gPrevImg.SampleLevel( linearClamp, float2( uv3.x, uv12.y ), 0 ).rgb * ( w3.x * w12.y );
    result += gPrevImg.SampleLevel( linearClamp, float2( uv12.x, uv3.y ), 0 ).rgb * ( w12.x * w3.y );

    const float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max( result / weight, float3( 0.0 ) ); // Catmull-Rom overshoots below zero
}

/// @brief Clips the history towards the box center (keeps the hue better than a per channel clamp)
float3 clipToBox( float3 history, float3 boxMin, float3 boxMax ) {
    const float3 center  = 0.5 * ( boxMax + boxMin );
    const float3 extents = 0.5 * ( boxMax - boxMin ) + 1e-5;
    const float3 offset  = history - center;
    const float3 units   = abs( offset / extents );
    const float  maxUnit = max( units.x, max( units.y, units.z ) );
    return maxUnit > 1.0 ? center + offset / maxUnit : history;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void computeMain( uint3 dispatchThreadId : SV_DispatchThreadID ) {
    uint width, height;
    gOutput.GetDimensions( width, height );

    const int2 pixel = int2( dispatchThreadId.xy );
    if ( pixel.x >= int( width ) || pixel.y >= int( height ) )
        return;

    const float3 currentHDR = gCurrImg.Load( int3( pixel, 0 ) ).rgb;
    if ( gPush.resetHistory != 0 )
    {
        gOutput[uint2( pixel )] = float4( currentHDR, 1.0 );
        return;
    }

    // 1. Neighborhood (3x3): moments for the variance box + longest velocity (edges take the motion of the moving object)
    const int2 maxPixel = int2( width, height ) - 1;

    float3 current       = float3( 0.0 );
    float3 m1            = float3( 0.0 );
    float3 m2            = float3( 0.0 );
    float3 boxMin        = float3( 1e10 );
    float3 boxMax        = float3( -1e10 );
    float2 velocity      = float2( 0.0 );
    float  velocityLenSq = -1.0;

    [unroll]
    for ( int y = -1; y <= 1; ++y )
    {
        [unroll]
        for ( int x = -1; x <= 1; ++x )
        {
            const int2   p = clamp( pixel + int2( x, y ), int2( 0, 0 ), maxPixel );
            const float3 c = toWorking( gCurrImg.Load( int3( p, 0 ) ).rgb );
            if ( x == 0 && y == 0 )
                current = c;

            m1 += c;
            m2 += c * c;
            boxMin = min( boxMin, c );
            boxMax = max( boxMax, c );

            const float2 v     = gVelocity.Load( int3( p, 0 ) );
            const float  lenSq = dot( v, v );
            if ( lenSq > velocityLenSq )
            {
                velocity      = v;
                velocityLenSq = lenSq;
            }
        }
    }

    const float3 mean  = m1 / 9.0;
    const float3 sigma = sqrt( max( m2 / 9.0 - mean * mean, float3( 0.0 ) ) );
    boxMin             = max( boxMin, mean - VARIANCE_GAMMA * sigma );
    boxMax             = min( boxMax, mean + VARIANCE_GAMMA * sigma );

    // 2. Reprojection. Off-screen in the previous frame -> no history
    const float2 uv        = ( float2( pixel ) + 0.5 ) * gPush.inverseTexSize;
    const float2 historyUV = uv - velocity;
    if ( any( historyUV < 0.0 ) || any( historyUV > 1.0 ) )
    {
        gOutput[uint2( pixel )] = float4( currentHDR, 1.0 );
        return;
    }

    float3 historyHDR = sampleHistoryCatmullRom( getSampler( gPush.samplerID ), historyUV );
    if ( any( isnan( historyHDR ) ) ) // A NaN would live forever in the history
        historyHDR = currentHDR;

    const float3 history = clipToBox( toWorking( historyHDR ), boxMin, boxMax );

    // 3. Blend: more weight to the current frame when moving (less ghosting and blur)
    const float speed = length( velocity * rcp( gPush.inverseTexSize ) ); // Pixels / frame
    const float alpha = lerp( BASE_ALPHA, MOTION_ALPHA, saturate( speed / MOTION_PIXELS ) );

    const float3 resolved   = fromWorking( lerp( history, current, alpha ) );
    gOutput[uint2( pixel )] = float4( max( resolved, float3( 0.0 ) ), 1.0 );
}
