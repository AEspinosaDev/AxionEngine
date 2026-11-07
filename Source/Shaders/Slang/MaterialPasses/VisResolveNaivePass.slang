#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;
import VisObject;
#include "Vis.slang" // computeBarycentrics, loadVisSurface

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

/// @brief Surface of a pixel of this dispatch bundle. False if the pixel belongs to another material bundle
bool loadOwnVisSurface<O : IVisObject>( O object, uint triangleID, float2 ndc, out SurfaceData surface, out uint materialID ) {
    surface    = {};
    materialID = object.getMaterialID();
    if ( getMaterialData( materialID ).psoBundleID != gPush.psoBundleID )
        return false;

    loadVisSurface( object, triangleID, ndc, surface, materialID );
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
        ownPixel = loadOwnVisSurface( VisParticle( instanceID ), triangleID, ndc, surface, materialID );
    else
        ownPixel = loadOwnVisSurface( VisInstance( instanceID - 1u ), triangleID, ndc, surface, materialID );

    if ( !ownPixel )
        return; // Pixel belongs to another material bundle

    T material;
    material.init( materialID );

    gOutColor[pixelCoord] = float4( material.evalColor( surface ), 1.0 );
}
