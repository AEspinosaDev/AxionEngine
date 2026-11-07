#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;
import IMaterial;
import VisObject;
#include "Vis.slang" // computeBarycentrics, loadVisSurface

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_TEXTURE( Texture2D<uint2>, gVisBuffer, 0 );
DECLARE_BUFFER( StructuredBuffer<uint>, gPixelList, 0 ); // x | y << 16, grouped by bin
DECLARE_BUFFER( StructuredBuffer<uint>, gBinOffset, 1 );
DECLARE_BUFFER( StructuredBuffer<uint>, gBinCount, 2 );
DECLARE_RW_TEXTURE( RWTexture2D<float4>, gOutColor, 0 );

// Indirect contract: binID must be the first field (written by the dispatch command signature)
struct ResolvePushConstant {
    uint binID;
};
DECLARE_PUSH_CONSTANT( ResolvePushConstant, gPush );

// -----------------------------------------------------------
// SHADER WITH SPECIALIZATION PER MATERIAL !
// One thread per pixel of the bin: every pixel that arrives belongs to this PSO
[shader( "compute" )]
[numthreads( RESOLVE_BIN_GROUP_SIZE, 1, 1 )]
void csResolve<T : IMaterial>( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    // Slot of the thread inside its bin (the last group has a tail)
    const uint slot = dispatchThreadID.x;
    if ( slot >= gBinCount[gPush.binID] )
        return;

    const uint2  pixelCoord = unpackBinPixel( gPixelList[gBinOffset[gPush.binID] + slot] );
    const float2 resolution = gFrame.getResolution();

    uint2 visData    = gVisBuffer.Load( int3( pixelCoord, 0 ) );
    uint  triangleID = visData.x; // ( meshlet << VIS_TRIANGLE_BITS ) | meshlet local triangle
    uint  instanceID = visData.y; // Never background here: classify leaves it out of the bins

    // Pixel center to NDC (D3D: screen Y goes down, NDC Y goes up)
    float2 ndc = ( ( float2( pixelCoord ) + 0.5 ) / resolution ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 );

    SurfaceData surface;
    uint        materialID;
    if ( isVisParticle( instanceID ) )
        loadVisSurface( VisParticle( instanceID ), triangleID, ndc, surface, materialID );
    else
        loadVisSurface( VisInstance( instanceID - 1u ), triangleID, ndc, surface, materialID );

    T material;
    material.init( materialID );

    gOutColor[pixelCoord] = float4( material.evalColor( surface ), 1.0 );
}
