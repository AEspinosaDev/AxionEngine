

RWTexture2D<float4> gOutput : register(u0, space0);
RaytracingAccelerationStructure gRtScene : register(t0, space0);

struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void rayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim   = DispatchRaysDimensions().xy;

    float2 crd = float2(launchIndex) + 0.5;
    float2 dims = float2(launchDim);

    float2 d = ((crd / dims) * 2.f - 1.f);
    
    // Setup camera ray (Simplified for example)
    // In real code, you read the Camera UBO here to get origin/direction
    RayDesc ray;
    ray.Origin = float3(0, 0, -2); // Example pos
    ray.Direction = normalize(float3(d.x, -d.y, 1)); 
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float4(0, 0, 0, 0);

    TraceRay(
        gRtScene,
        RAY_FLAG_NONE,
        0xFF, // Mask
        0,    // Hit Group Index (offset in SBT)
        0,    // Geometry Multiplier
        0,    // Miss Shader Index
        ray,
        payload
    );

    gOutput[launchIndex] = payload.color;
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    // Background color (e.g., dark blue)
    payload.color = float4(0.0, 0.1, 0.3, 1.0);
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // Basic Barycentric visualization for the cube
    float3 barycentric = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    payload.color = float4(barycentric, 1.0);
}