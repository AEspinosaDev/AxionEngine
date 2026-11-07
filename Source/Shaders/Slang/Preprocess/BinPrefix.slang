#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;   // Binning constants
import IndirectCommand;  // DispatchIndirectCommand

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gBinCount, 0 );
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gBinOffset, 0 );                       // First slot of each bin in the pixel list
DECLARE_RW_BUFFER( RWStructuredBuffer<DispatchIndirectCommand>, gDispatchArgs, 1 ); // Resolve dispatch of each bin

struct BinPrefixPushConstant {
    uint binCount; ///< Bins used this frame
};
DECLARE_PUSH_CONSTANT( BinPrefixPushConstant, gPush );

groupshared uint gsBinCount[MAX_RESOLVE_BINS];

[shader( "compute" )]
[numthreads( MAX_RESOLVE_BINS, 1, 1 )]
void csBinPrefix( uint bin: SV_GroupIndex ) {
    const uint count = bin < gPush.binCount ? gBinCount[bin] : 0;
    gsBinCount[bin]  = count;
    GroupMemoryBarrierWithGroupSync();

    // Exclusive scan. 64 threads are two waves on NVIDIA (WavePrefixSum stops at the wave): a serial sum is enough for so few bins
    uint offset = 0;
    for ( uint i = 0; i < bin; ++i )
        offset += gsBinCount[i];

    if ( bin >= gPush.binCount )
        return;

    gBinOffset[bin] = offset;

    DispatchIndirectCommand args;
    args.baseInstanceID    = bin; // Root constant: the resolve reads its bin from the push constant
    args.threadGroupCountX = ( count + RESOLVE_BIN_GROUP_SIZE - 1 ) / RESOLVE_BIN_GROUP_SIZE;
    args.threadGroupCountY = 1;
    args.threadGroupCountZ = 1;
    gDispatchArgs[bin]     = args;
}
