#include "Montecarlo.slang"
#include "Random.slang"

RaytracingAccelerationStructure gAccel : register( t0, space0 );
RWTexture2D<float4>             gOutput : register( u0, space0 );

struct SceneData {
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
    float4x4 model;
    uint     frameIndex;
};

cbuffer CameraCB : register( b0, space0 ) {
    SceneData gScene;
};


struct RayPayload {
    float3 albedo;
    float3 normal;
    float3 emission;
    float  t; // Hit dist
    bool   isHit;
};

struct BuiltInTriangleIntersectionAttributes {
    float2 barycentrics;
};

[shader( "raygeneration" )]
void raygenMain() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims  = DispatchRaysDimensions().xy;

    float2 uv        = ( float2( pixel ) + 0.5 ) / float2( dims );
    float2 screenPos = uv * 2.0 - 1.0;
    screenPos.y      = -screenPos.y; // Flip Y for DX12

    uint seed = pixel.y * dims.x + pixel.x + gScene.frameIndex * 719393;

    float3 pixelColor = float3( 0, 0, 0 );

    const int SAMPLES_PER_PIXEL = 16;
    const int MAX_BOUNCES       = 8;

    for ( int s = 0; s < SAMPLES_PER_PIXEL; ++s )
    {
        // AA with jitter
        float4 target     = mul( gScene.invProj, float4( screenPos.x, screenPos.y, 1, 1 ) );
        float3 rayDirView = normalize( target.xyz / target.w );
        float3 rayOrigin  = float3( gScene.invView[0].w, gScene.invView[1].w, gScene.invView[2].w );
        float3 rayDir     = normalize( mul( (float3x3)gScene.invView, rayDirView ) );

        RayDesc ray;
        ray.Origin    = rayOrigin;
        ray.Direction = rayDir;
        ray.TMin      = 0.001;
        ray.TMax      = 10000.0;

        float3 sampleColor = 0.0;
        float3 throughput  = 1.0;

        RayPayload payload;

        // --- (PATH TRACING) ---
        for ( int depth = 0; depth < MAX_BOUNCES; ++depth )
        {
            payload.isHit    = false;
            payload.emission = float3( 0.0 ); // Reset importante

            TraceRay( gAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload );

            if ( !payload.isHit )
            {
                sampleColor += throughput * float3( 0.02, 0.02, 0.05 ); // Cielo
                break;
            }

            // Luz emitida
            sampleColor += throughput * payload.emission;

            if ( length( payload.emission ) > 1.0 )
                break;

            // Material Eval
            throughput *= payload.albedo;

            // Roulette
            if ( depth > 2 )
            {
                float p = max( throughput.r, max( throughput.g, throughput.b ) );
                if ( randomFloat( seed ) > p )
                    break;
                throughput /= p;
            }

            float2   xi          = float2( randomFloat( seed ), randomFloat( seed ) );
            float4   localSample = cosineSampleHemisphere( xi );
            float3x3 tbn         = getTangentBasis( normalize( payload.normal ) );
            float3   bounceDir   = normalize( mul( localSample.xyz, tbn ) );

            ray.Origin    = ray.Origin + ray.Direction * payload.t + ( payload.normal * 0.001 );
            ray.Direction = bounceDir;
            ray.TMin      = 0.001;
            ray.TMax      = 10000.0;
        }

        pixelColor += sampleColor;
    }

    // Montecarlo
    float3 currentFrameColor = pixelColor / float( SAMPLES_PER_PIXEL );

    // Cheap Temp. Accum
    if ( gScene.frameIndex == 0 )
    {
        gOutput[pixel] = float4( currentFrameColor, 1.0 );
    } else
    {
        float3 historyColor = gOutput[pixel].rgb;

        float  weight     = 1.0 / ( float( gScene.frameIndex ) + 1.0 );
        float3 finalColor = lerp( historyColor, currentFrameColor, weight );

        gOutput[pixel] = float4( finalColor, 1.0 );
    }
}

[shader( "miss" )]
void missMain( inout RayPayload payload ) {
    payload.isHit = false;
}

// Material 1: Red
[shader( "closesthit" )]
void closestHit( inout RayPayload payload, BuiltInTriangleIntersectionAttributes attr ) {
    payload.isHit = true;
    payload.t     = RayTCurrent();

    float3 objNormal = float3( 0, 1, 0 ); // Placeholder

    payload.normal = normalize( -WorldRayDirection() );

    uint id          = InstanceID();
    payload.emission = float3( 0, 0, 0 );

    if ( id == 0 )
        payload.albedo = float3( 0.8, 0.8, 0.8 ); // Floor
    else if ( id == 1 )
        payload.albedo = float3( 0.8, 0.8, 0.8 ); // Ceiling
    else if ( id == 2 )
        payload.albedo = float3( 0.8, 0.8, 0.8 ); // Center
    else if ( id == 3 )
        payload.albedo = float3( 0.8, 0.1, 0.1 ); // Left (Red)
    else if ( id == 4 )
        payload.albedo = float3( 0.1, 0.8, 0.1 ); // Right (Green)
    else if ( id == 5 )
    { // Light
        payload.albedo   = float3( 0, 0, 0 );
        payload.emission = float3( 20.0, 18.0, 15.0 ); // Strong warm light
    } else
        payload.albedo = float3( 0.5, 0.5, 0.5 ); // Cubes
}
