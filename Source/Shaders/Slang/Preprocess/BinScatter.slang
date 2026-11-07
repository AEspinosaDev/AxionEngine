#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;   // gFrame, binning constants, packBinPixel

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_TEXTURE( Texture2D<uint>, gBinImage, 0 );
DECLARE_BUFFER( StructuredBuffer<uint>, gBinOffset, 0 );
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gBinCursor, 0 ); // Reserved slots per bin, relative to gBinOffset (zeroed by BinClearPass)
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gPixelList, 1 ); // x | y << 16, grouped by bin

groupshared uint gsBinCount[MAX_RESOLVE_BINS];
groupshared uint gsBinBase[MAX_RESOLVE_BINS];

[shader( "compute" )]
[numthreads( BINNING_TILE_SIZE, BINNING_TILE_SIZE, 1 )]
void csBinScatter( uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex ) {
    if ( groupIndex < MAX_RESOLVE_BINS )
        gsBinCount[groupIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    // 1. Rank of the pixel inside its bin, in this tile
    const uint2 pixelCoord = dispatchThreadID.xy;
    const uint  bin        = all( pixelCoord < (uint2)gFrame.getResolution() ) ? gBinImage.Load( int3( pixelCoord, 0 ) ) : BIN_NONE;

    uint rank = 0;
    if ( bin != BIN_NONE )
        InterlockedAdd( gsBinCount[bin], 1, rank );
    GroupMemoryBarrierWithGroupSync();

    // 2. One reservation per bin of the tile: a contiguous block of slots
    if ( groupIndex < MAX_RESOLVE_BINS && gsBinCount[groupIndex] > 0 )
    {
        uint base;
        InterlockedAdd( gBinCursor[groupIndex], gsBinCount[groupIndex], base );
        gsBinBase[groupIndex] = gBinOffset[groupIndex] + base;
    }
    GroupMemoryBarrierWithGroupSync();

    // 3. Pixel -> its slot
    if ( bin != BIN_NONE )
        gPixelList[gsBinBase[bin] + rank] = packBinPixel( pixelCoord );
}
