#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;   // gFrame, gInstances, gMaterials, gParticleSystems, binning constants
import VisObject;        // isVisParticle

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_TEXTURE( Texture2D<uint2>, gVisBuffer, 0 );
DECLARE_BUFFER( StructuredBuffer<uint>, gBundleToBin, 0 );   // psoBundleID -> bin (BIN_NONE if not drawn)
DECLARE_RW_TEXTURE( RWTexture2D<uint>, gBinImage, 0 );       // Bin of every pixel
DECLARE_RW_TEXTURE( RWTexture2D<float4>, gOutColor, 1 );     // Background and pixels without bin only
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gBinCount, 0 ); // Pixels per bin

groupshared uint gsBinCount[MAX_RESOLVE_BINS]; // Tile histogram

/// @brief Bin of a visibility buffer pixel. Particles take the material of their system
uint getPixelBin( uint instanceID ) {
    uint materialID;
    if ( isVisParticle( instanceID ) )
    {
        uint systemIndex, particleIndex;
        unpackVisibilityParticle( instanceID, systemIndex, particleIndex );
        materialID = gParticleSystems[systemIndex].materialID;
    } else
        materialID = getInstanceData( instanceID - 1u ).materialID;

    return gBundleToBin[getMaterialData( materialID ).psoBundleID];
}

[shader( "compute" )]
[numthreads( BINNING_TILE_SIZE, BINNING_TILE_SIZE, 1 )]
void csBinClassify( uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex ) {
    // 1. Empty histogram: one thread per bin
    if ( groupIndex < MAX_RESOLVE_BINS )
        gsBinCount[groupIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    // 2. Bin of the pixel. Out of screen and background pixels have none
    const uint2 pixelCoord = dispatchThreadID.xy;
    uint        bin        = BIN_NONE;
    if ( all( pixelCoord < (uint2)gFrame.getResolution() ) )
    {
        const uint instanceID = gVisBuffer.Load( int3( pixelCoord, 0 ) ).y; // Instance offset by 1 (0 = background), or particle
        if ( instanceID == 0 )
            gOutColor[pixelCoord] = float4( 0.1, 0.1, 0.2, 1.0 ); // Background resolved here
        else
        {
            bin = getPixelBin( instanceID );
            if ( bin == BIN_NONE )
                gOutColor[pixelCoord] = float4( 1.0, 0.0, 1.0, 1.0 ); // Drawn but without resolve PSO: visible on purpose
        }
        gBinImage[pixelCoord] = bin;
    }

    if ( bin != BIN_NONE )
        InterlockedAdd( gsBinCount[bin], 1 );
    GroupMemoryBarrierWithGroupSync();

    // 3. One global atomic per bin present in the tile
    if ( groupIndex < MAX_RESOLVE_BINS && gsBinCount[groupIndex] > 0 )
        InterlockedAdd( gBinCount[groupIndex], gsBinCount[groupIndex] );
}
