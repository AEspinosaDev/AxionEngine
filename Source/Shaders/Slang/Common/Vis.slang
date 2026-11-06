// Visibility logic shared by the instances (VisPass) and the particles (VisParticlePass).
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
