struct Vertex {
    float3 pos;
    float2 uv;
};

struct SceneData {
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
    float4x4 model;
    uint     frameIndex;
};

RaytracingAccelerationStructure gAccel : register( t0, space0 );
RWTexture2D<float4>             gOutput : register( u0, space0 );
cbuffer                         CameraCB : register( b0, space0 ) {
    SceneData gScene;
};

struct RayPayload {
    float3 albedo;
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

    float3 pixelColor = float3( 0, 0, 0 );

    // AA with jitter
    float4 target     = mul( gScene.invProj, float4( screenPos.x, screenPos.y, 1, 1 ) );
    float3 rayDirView = normalize( target.xyz / target.w );
    float3 rayOrigin  = float3( gScene.invView[0].w, gScene.invView[1].w, gScene.invView[2].w );
    float3 rayDir     = normalize( mul( (float3x3)gScene.invView, rayDirView ) );

    RayDesc ray;
    ray.Origin    = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin      = 0.001;
    ray.TMax      = 1000.0;

    float3 sampleColor = 0.0;
    float3 throughput  = 1.0;

    RayPayload payload;
    TraceRay( gAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload );

    gOutput[pixel] = float4( payload.albedo, 1.0 );
}

[shader( "miss" )]
void missMain( inout RayPayload payload ) {
    payload.albedo = float3( 0.1 );
}

[shader( "closesthit" )]
void hitMain( inout RayPayload payload, BuiltInTriangleIntersectionAttributes attr ) {
    payload.albedo = float3( 0.9, 0.0, 0.0 );
}

