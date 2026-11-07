// Visibility logic shared by the instances (VisPass), the particles (VisParticlePass) and the resolves (surface of a pixel).
// #include it after importing MaterialCommon, IMaterial and VisObject: psVis is an entry point, so it must stay in the module of each pass.

// One mesh shading group per meshlet: enough threads for all its vertices and triangles without looping
static const uint MS_GROUP_SIZE = 128; // >= max( MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES )

struct VisVertex {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    // For velocity computation
    float4 currentPos : CURR_POS;
    float4 prevPos : PREV_POS;
};

// Per primitive attributes: the triangle id comes from the mesh shader, not from SV_PrimitiveID (~2.6x slower on NVIDIA here)
struct VisPrimitive {
    nointerpolation uint triangleID : TRIANGLE_ID; // ( meshlet << VIS_TRIANGLE_BITS ) | local triangle
    nointerpolation uint instanceID : INSTANCE_ID; // Instance offset by 1 (0 = background), or packVisibilityParticle
    nointerpolation uint materialID : DATA0;
};

struct PSOutput {
    uint2  visibility : SV_Target0; // RT0: R32G32_UINT (VisBuffer)
    float2 velocity : SV_Target1;   // RT1: R16G16_FLOAT (Velocity)
};

// Mesh shader vertex (vertex pulling through the meshlet vertex list), placed in the world by its owner
VisVertex buildVisVertex<T : IMaterial, O : IVisObject>( O object, MeshData mesh, Meshlet meshlet, uint vertexIndex ) {
    CanonVertex v        = CanonVertex.fromIndex( getMeshletVertexIndex( mesh, meshlet, vertexIndex ), mesh );
    float4      worldPos = object.placePosition( v.position );

    // WPO
    T material;
    material.init( object.getMaterialID() );
    SurfaceData surface;
    surface.worldPos = worldPos.xyz;
    surface.shNormal = normalize( object.placeNormal( v.normal ) );
    surface.uv       = v.uv;
    worldPos.xyz += material.computeWPO( surface );

    float4 clipPos = mul( gFrame.viewProj, worldPos );

    VisVertex output;
    output.position   = clipPos;
    output.uv         = v.uv;
    output.currentPos = clipPos;
    output.prevPos    = clipPos; // Velocity TBD: prevModelMatrix / prevViewProj
    return output;
}

// Mesh shader primitive
VisPrimitive buildVisPrimitive( uint meshletIndex, uint localTriangle, uint instanceID, uint materialID ) {
    VisPrimitive prim;
    prim.triangleID = packVisibilityTriangle( meshletIndex, localTriangle );
    prim.instanceID = instanceID;
    prim.materialID = materialID;
    return prim;
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "fragment" )]
PSOutput psVis<T : IMaterial>( VisVertex input, VisPrimitive prim ) {
    PSOutput output;

    // Alpha clipping
    T material;
    material.init( prim.materialID );

    SurfaceData surface;
    surface.uv = input.uv;
    if ( material.evalOpacity( surface ) < 0.5 )
    {
        discard;
    }

    // 1. VISIBILITY BUFFER (Target 0)
    output.visibility = uint2( prim.triangleID, prim.instanceID );

    // 2. VELOCITY BUFFER (Target 1)
    float2 currentNDC = input.currentPos.xy / input.currentPos.w;
    float2 prevNDC    = input.prevPos.xy / input.prevPos.w;
    // TODO: Check if I need to normalize these
    output.velocity = currentNDC - prevNDC;

    return output;
}

// -----------------------------------------------------------
// RESOLVE: surface of a visibility buffer pixel
// -----------------------------------------------------------

/// @brief Perspective-correct barycentrics of a NDC point inside a clip-space triangle
float3 computeBarycentrics( float4 p0, float4 p1, float4 p2, float2 ndc ) {
    float3 invW = rcp( float3( p0.w, p1.w, p2.w ) );
    float2 ndc0 = p0.xy * invW.x;
    float2 ndc1 = p1.xy * invW.y;
    float2 ndc2 = p2.xy * invW.z;

    float  invDet = rcp( determinant( float2x2( ndc2 - ndc1, ndc0 - ndc1 ) ) );
    float3 ddx    = float3( ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y ) * invDet * invW;
    float3 ddy    = float3( ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x ) * invDet * invW;

    float2 delta      = ndc - ndc0;
    float  interpInvW = invW.x + delta.x * dot( ddx, float3( 1.0 ) ) + delta.y * dot( ddy, float3( 1.0 ) );
    float  interpW    = rcp( interpInvW );

    return interpW * float3( invW.x + delta.x * ddx.x + delta.y * ddy.x,
                             delta.x * ddx.y + delta.y * ddy.y,
                             delta.x * ddx.z + delta.y * ddy.z );
}

/// @brief Surface of a pixel, seen through its owner. Specialized per owner at compile time: no more questions about what it is
void loadVisSurface<O : IVisObject>( O object, uint triangleID, float2 ndc, out SurfaceData surface, out uint materialID ) {
    surface    = {};
    materialID = object.getMaterialID();

    MeshData mesh = getMeshData( object.getMeshID() );

    uint meshletIndex, localTriangle;
    unpackVisibilityTriangle( triangleID, meshletIndex, localTriangle );
    Meshlet meshlet = getMeshlet( mesh, meshletIndex );
    uint3   tri     = getMeshletTriangle( mesh, meshlet, localTriangle );

    CanonVertex v0 = CanonVertex.fromIndex( getMeshletVertexIndex( mesh, meshlet, tri.x ), mesh );
    CanonVertex v1 = CanonVertex.fromIndex( getMeshletVertexIndex( mesh, meshlet, tri.y ), mesh );
    CanonVertex v2 = CanonVertex.fromIndex( getMeshletVertexIndex( mesh, meshlet, tri.z ), mesh );

    // TODO: WPO is applied in the vis pass but not here (no material uses it yet)
    float4 wp0 = object.placePosition( v0.position );
    float4 wp1 = object.placePosition( v1.position );
    float4 wp2 = object.placePosition( v2.position );

    float3 bary = computeBarycentrics( mul( gFrame.viewProj, wp0 ), mul( gFrame.viewProj, wp1 ), mul( gFrame.viewProj, wp2 ), ndc );

    float3 localNrm = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;

    surface.worldPos = wp0.xyz * bary.x + wp1.xyz * bary.y + wp2.xyz * bary.z;
    surface.shNormal = normalize( object.placeNormal( localNrm ) );
    surface.uv       = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;
    surface.V        = normalize( gFrame.getCameraPos() - surface.worldPos );
}
