
// ============================================================================
// NVIDIA FXAA 3.11 by TIMOTHY LOTTES
// ------------------------------------------------------------------------------
// COPYRIGHT (C) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.
// ------------------------------------------------------------------------------
// TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
// *AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL NVIDIA
// OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR
// LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION,
// OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE
// THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGES.

// ============================================================================
// High-Quality FXAA 3.11 Compute Shader (PC Preset)
// ============================================================================
#include "Defines.slang"

Texture2D<float4>   gInput : register( t0, space0 );
RWTexture2D<float4> gOutput : register( u1, space0 );
SamplerState gSampler: register(s0, space0);

public struct PushConstant {
    float  subpix;
    float  edgeThreshold;
    float  edgeThresholdMin;
    float  pad;
    float2 inverseTexSize;
};

PUSH_CONSTANT( PushConstant, gPush, b0, space1 );

#if defined( FXAA_QUALITY_LOW )
#define FXAA_SEARCH_STEPS 4
#define FXAA_SEARCH_ACCELERATION 2.0
#define FXAA_SEARCH_THRESHOLD 0.33
#elif defined( FXAA_QUALITY_MEDIUM )
#define FXAA_SEARCH_STEPS 8
#define FXAA_SEARCH_ACCELERATION 1.5
#define FXAA_SEARCH_THRESHOLD 0.25
#elif defined( FXAA_QUALITY_ULTRA )
#define FXAA_SEARCH_STEPS 32
#define FXAA_SEARCH_ACCELERATION 1.0
#define FXAA_SEARCH_THRESHOLD 0.125
#else
// Default: QUALITY_HIGH
#define FXAA_SEARCH_STEPS 12
#define FXAA_SEARCH_ACCELERATION 1.5
#define FXAA_SEARCH_THRESHOLD 0.25
#endif

float getLuma(float3 rgb) {
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void computeMain( uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint  width, height;
    gOutput.GetDimensions( width, height );

    uint2 pixelCoord = dispatchThreadId.xy;
    if ( pixelCoord.x >= width || pixelCoord.y >= height )
        return;

    float2 uv = ( float2( pixelCoord ) + 0.5 ) * gPush.inverseTexSize;

    // 1. Local Contrast Check
    float3 rgbM  = gInput.SampleLevel( gSampler, uv, 0 ).rgb;
    float  lumaM = getLuma( rgbM );

    float lumaS = getLuma( gInput.SampleLevel( gSampler, uv + float2( 0, 1 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaN = getLuma( gInput.SampleLevel( gSampler, uv + float2( 0, -1 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaE = getLuma( gInput.SampleLevel( gSampler, uv + float2( 1, 0 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaW = getLuma( gInput.SampleLevel( gSampler, uv + float2( -1, 0 ) * gPush.inverseTexSize, 0 ).rgb );

    float lumaMax   = max( lumaM, max( max( lumaN, lumaS ), max( lumaW, lumaE ) ) );
    float lumaMin   = min( lumaM, min( min( lumaN, lumaS ), min( lumaW, lumaE ) ) );
    float lumaRange = lumaMax - lumaMin;

    if ( lumaRange < max( gPush.edgeThresholdMin, lumaMax * gPush.edgeThreshold ) ) {
        gOutput[pixelCoord] = float4( rgbM, 1.0 );
        return;
    }

    // 2. Sub-pixel aliasing test 
    float lumaNW = getLuma( gInput.SampleLevel( gSampler, uv + float2( -1, -1 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaNE = getLuma( gInput.SampleLevel( gSampler, uv + float2( 1, -1 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaSW = getLuma( gInput.SampleLevel( gSampler, uv + float2( -1, 1 ) * gPush.inverseTexSize, 0 ).rgb );
    float lumaSE = getLuma( gInput.SampleLevel( gSampler, uv + float2( 1, 1 ) * gPush.inverseTexSize, 0 ).rgb );

    // Filtro de sub-píxel de bajo coste (3x3 Box Filter)
    float lumaL       = ( lumaN + lumaS + lumaE + lumaW );
    float blendFactor = ( ( lumaL * 2.0 ) + ( lumaNW + lumaNE + lumaSW + lumaSE ) ) / 12.0;
    blendFactor       = saturate( abs( blendFactor - lumaM ) / lumaRange );
    blendFactor       = smoothstep( 0, 1, blendFactor );
    blendFactor       = blendFactor * blendFactor * gPush.subpix;

    // 3. Edge Direction Detection
    float edgeHorz = abs(-2.0 * lumaW + lumaNW + lumaSW) +
                     abs(-2.0 * lumaM + lumaN  + lumaS ) * 2.0 +
                     abs(-2.0 * lumaE + lumaNE + lumaSE);
                     
    float edgeVert = abs(-2.0 * lumaN + lumaNW + lumaNE) +
                     abs(-2.0 * lumaM + lumaW  + lumaE ) * 2.0 +
                     abs(-2.0 * lumaS + lumaSW + lumaSE);
                     
    bool isHorizontal = (edgeHorz >= edgeVert);

    float lengthSign = isHorizontal ? gPush.inverseTexSize.y : gPush.inverseTexSize.x;
    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);

    bool is1Steepest = gradient1 >= gradient2;
    float gradientScaled = 0.25 * max(gradient1, gradient2);
    float stepDirection = is1Steepest ? -lengthSign : lengthSign;
    float lumaLocalAvg = 0.5 * (lumaM + (is1Steepest ? luma1 : luma2));

    float2 currentUv = uv;
    if (isHorizontal) {
        currentUv.y += stepDirection * 0.5;
    } else {
        currentUv.x += stepDirection * 0.5;
    }

    // 4. Edge Exploration (The core of high-quality FXAA)
    float2 offset = isHorizontal ? float2(gPush.inverseTexSize.x, 0.0) : float2(0.0, gPush.inverseTexSize.y);
    float2 uv1 = currentUv - offset;
    float2 uv2 = currentUv + offset;

    float lumaEnd1 = getLuma( gInput.SampleLevel( gSampler, uv1, 0 ).rgb ) - lumaLocalAvg;
    float lumaEnd2 = getLuma( gInput.SampleLevel( gSampler, uv2, 0 ).rgb ) - lumaLocalAvg;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    [unroll]
    for (int i = 2; i < FXAA_SEARCH_STEPS; i++) {
        if (reachedBoth) break;
        
        if (!reached1) {
            lumaEnd1 = getLuma(gInput.SampleLevel(gSampler, uv1, 0).rgb) - lumaLocalAvg;
            reached1 = abs(lumaEnd1) >= gradientScaled;
            if (!reached1) uv1 -= offset * FXAA_SEARCH_ACCELERATION;
        }
        if (!reached2) {
            lumaEnd2 = getLuma(gInput.SampleLevel(gSampler, uv2, 0).rgb) - lumaLocalAvg;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            if (!reached2) uv2 += offset * FXAA_SEARCH_ACCELERATION;
        }
    }

    // 5. Calculate final offset
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;

    float pixelOffset = -distanceFinal / edgeThickness + 0.5;
    bool isLumaCenterSmaller = lumaM < lumaLocalAvg;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;

    float finalOffset = correctVariation ? pixelOffset : 0.0;
    finalOffset = max(finalOffset, blendFactor);

    // 6. Sample final pixel
    float2 finalUv = uv;
    if (isHorizontal) {
        finalUv.y += finalOffset * stepDirection;
    } else {
        finalUv.x += finalOffset * stepDirection;
    }

    float3 finalColor = gInput.SampleLevel( gSampler, finalUv, 0 ).rgb;
    gOutput[pixelCoord] = float4(finalColor, 1.0);
}