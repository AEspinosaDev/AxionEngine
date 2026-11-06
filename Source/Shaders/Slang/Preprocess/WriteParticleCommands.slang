#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
import MaterialCommon;   // gMeshes, gParticleSystems, getParticleDispatchGrid
import IndirectCommand;

static const uint WRITE_COMMANDS_GROUP_SIZE = 64; // Must match WriteParticleCommandsPass::GROUP_SIZE
static const uint INVALID_GEOMETRY_PAGE     = 0xFFFFFFFF;

// -----------------------------------------------------------
// PASS I/O (Space 1)
// -----------------------------------------------------------
DECLARE_BUFFER( StructuredBuffer<uint>, gVisibleCounts, 0 );                        // Visible particles per system
DECLARE_RW_BUFFER( RWStructuredBuffer<DispatchMeshIndirectCommand>, gCommands, 0 ); // Particle commands go after the instance ones

struct WriteCommandsPushConstant {
    uint systemCount;
    uint firstCommand;
};
DECLARE_PUSH_CONSTANT( WriteCommandsPushConstant, gPush );

[shader( "compute" )]
[numthreads( WRITE_COMMANDS_GROUP_SIZE, 1, 1 )]
void csWriteParticleCommands( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    const uint systemIndex = dispatchThreadID.x;
    if ( systemIndex >= gPush.systemCount )
        return;

    MeshData mesh = getMeshData( gParticleSystems[systemIndex].meshID );

    // A mesh not placed in a geometry page yet dispatches nothing: its heap IDs are invalid
    const uint  meshletCount = mesh.geometryPageID != INVALID_GEOMETRY_PAGE ? mesh.meshletCount : 0;
    const uint3 grid         = getParticleDispatchGrid( gVisibleCounts[systemIndex], meshletCount );

    DispatchMeshIndirectCommand command;
    command.baseInstanceID    = systemIndex;
    command.threadGroupCountX = grid.x;
    command.threadGroupCountY = grid.y;
    command.threadGroupCountZ = grid.z;

    gCommands[gPush.firstCommand + systemIndex] = command;
}
