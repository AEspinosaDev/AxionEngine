#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;
import VisObject;

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_TEXTURE( Texture2D<uint2>, gVisBuffer, 0 );
DECLARE_RW_TEXTURE( RWTexture2D<float4>, gOutColor, 0 );

// Naive resolve: one full-screen dispatch per material bundle, each one only shades its own pixels
struct ResolvePushConstant {
    uint psoBundleID;
};
DECLARE_PUSH_CONSTANT( ResolvePushConstant, gPush );

// -----------------------------------------------------------
// Perspective-correct barycentrics of a NDC point inside a clip-space triangle
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

// Surface of a pixel, seen through its owner. Specialized per owner at compile time: no more questions about what it is
// False if the pixel belongs to another material bundle
bool loadVisSurface<O : IVisObject>( O object, uint triangleID, float2 ndc, out SurfaceData surface, out uint materialID ) {
    surface    = {};
    materialID = object.getMaterialID();
    if ( getMaterialData( materialID ).psoBundleID != gPush.psoBundleID )
        return false;

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
    return true;
}

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void csResolve<T : IMaterial>( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    uint2  pixelCoord = dispatchThreadID.xy;
    float2 resolution = gFrame.getResolution();

    // Out of screen (resolution not multiple of the group size)
    if ( any( pixelCoord >= (uint2)resolution ) )
        return;

    uint2 visData     = gVisBuffer.Load( int3( pixelCoord, 0 ) );
    uint  triangleID  = visData.x; // ( meshlet << VIS_TRIANGLE_BITS ) | meshlet local triangle
    uint  instanceID  = visData.y; // Instance offset by 1 (0 = background), or particle

    if ( instanceID == 0 )
    {
        gOutColor[pixelCoord] = float4( 0.1, 0.1, 0.2, 1.0 ); // Every dispatch writes the same value
        return;
    }

    // Pixel center to NDC (D3D: screen Y goes down, NDC Y goes up)
    float2 ndc = ( ( float2( pixelCoord ) + 0.5 ) / resolution ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 );

    // The only question per pixel
    SurfaceData surface;
    uint        materialID;
    bool        ownPixel;
    if ( isVisParticle( instanceID ) )
        ownPixel = loadVisSurface( VisParticle( instanceID ), triangleID, ndc, surface, materialID );
    else
        ownPixel = loadVisSurface( VisInstance( instanceID - 1u ), triangleID, ndc, surface, materialID );

    if ( !ownPixel )
        return; // Pixel belongs to another material bundle

    T material;
    material.init( materialID );

    gOutColor[pixelCoord] = float4( material.evalColor( surface ), 1.0 );
}
