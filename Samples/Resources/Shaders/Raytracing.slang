RaytracingAccelerationStructure gScene : register(t0, space0);
RWTexture2D<float4> gOutput : register(u0, space0);

struct RayPayload
{
    float4 color;
};

struct BuiltInTriangleIntersectionAttributes
{
    float2 barycentrics;
};

[shader("raygeneration")]
void raygenMain()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim   = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim); 
    float2 screenPos = uv * 2.0 - 1.0;                            
    
    screenPos.y = -screenPos.y; 
    float aspectRatio = float(launchDim.x) / float(launchDim.y);
    screenPos.x *= aspectRatio;

    // --- Simple Cam ---
    RayDesc ray;
    ray.Origin    = float3(0.0, 0.0, -5.0);   // Z = -5
    ray.Direction = normalize(float3(screenPos.x, screenPos.y, 1.0)); // Z+ (FOV aprox 90)
    ray.TMin      = 0.001;                    
    ray.TMax      = 10000.0;                 

    RayPayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 0.0);

    TraceRay(
        gScene,            // Acceleration Structure
        RAY_FLAG_NONE,      // Flags
        0xFF,               // InstanceInclusionMask (0xFF = ver todo)
        0,                  // RayContributionToHitGroupIndex (Offset base en la SBT)
        1,                  // MultiplierForGeometryContributionToHitGroupIndex
        0,                  // MissShaderIndex (Offset en la tabla Miss)
        ray,                // El rayo
        payload             // Payload (inout)
    );

    gOutput[launchIndex] = payload.color;
}

[shader("miss")]
void missMain(inout RayPayload payload)
{
    payload.color = float4(0.2, 0.2, 0.2, 1.0);
}

// Material 1: Red
[shader("closesthit")]
void hitRedMain(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attr)
{
    payload.color = float4(1.0, 0.0, 0.0, 1.0);
}

// // Material 2: Blue
// [shader("closesthit")]
// void HitBlue(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attr)
// {
//     // Devolvemos azul sólido
//     payload.color = float4(0.0, 0.0, 1.0, 1.0);
// }