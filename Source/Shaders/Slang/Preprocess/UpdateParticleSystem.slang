#include "Defines.slang" // PUSH_CONSTANT / SPACE_* / DECLARE_* macros
#include "Random.slang"
import MaterialCommon;   // gFrame, gMeshes, gParticleSystems

static const uint  PARTICLE_GROUP_SIZE = 256; // Must match UpdateParticleSystemPass::GROUP_SIZE
static const float TWO_PI              = 6.28318530718;

// -----------------------------------------------------------
// PASS I/O (Space 1). The particles and the visible list of each system are bindless
// -----------------------------------------------------------
DECLARE_RW_BUFFER( RWStructuredBuffer<uint>, gVisibleCounts, 0 ); // Visible particles per system

struct UpdatePushConstant {
    uint  systemIndex;
    float deltaTime;
};
DECLARE_PUSH_CONSTANT( UpdatePushConstant, gPush );

// -----------------------------------------------------------
// ORBITALS. behaviorState: x = angle, y = radius, z = height, w = angular speed
// -----------------------------------------------------------
float3 getOrbitPosition( float4 orbit ) {
    return float3( cos( orbit.x ) * orbit.y, orbit.z, sin( orbit.x ) * orbit.y );
}

// Extent: x = inner radius, y = thickness, z = outer radius
void seedOrbital( ParticleSystemData system, inout uint rng, inout ParticleData particle ) {
    const float3 extent      = system.getExtent();
    const float  innerRadius = max( extent.x, 0.0001 );

    const float radius       = sqrt( lerp( innerRadius * innerRadius, extent.z * extent.z, randomFloat( rng ) ) ); // Uniform over the ring area
    const float angle        = randomFloat( rng ) * TWO_PI;
    const float height       = ( randomFloat( rng ) - 0.5 ) * extent.y;
    const float angularSpeed = pow( innerRadius / radius, 1.5 ); // Kepler: the inner orbits are faster

    particle.behaviorState = float4( angle, radius, height, angularSpeed );
    particle.position      = getOrbitPosition( particle.behaviorState );
}

void updateOrbital( ParticleSystemData system, inout ParticleData particle ) {
    const float angle        = particle.behaviorState.x + particle.behaviorState.w * system.speed * gPush.deltaTime;
    particle.behaviorState.x = frac( angle / TWO_PI ) * TWO_PI; // Wrapped: an always growing float loses precision
    particle.position        = getOrbitPosition( particle.behaviorState );
}

// -----------------------------------------------------------
// BEHAVIORS
// -----------------------------------------------------------
// Uniformly distributed over all the orientations (Shoemake)
float4 randomOrientation( inout uint rng ) {
    const float u = randomFloat( rng );
    const float a = randomFloat( rng ) * TWO_PI;
    const float b = randomFloat( rng ) * TWO_PI;

    const float r0 = sqrt( 1.0 - u );
    const float r1 = sqrt( u );
    return float4( r0 * sin( a ), r0 * cos( a ), r1 * sin( b ), r1 * cos( b ) );
}

ParticleData seedParticle( ParticleSystemData system, uint particleIndex ) {
    uint rng = ( particleIndex * 1664525u + 1013904223u ) ^ system.seed;
    nextRandom( rng );

    ParticleData particle;
    particle.position      = float3( 0.0 );
    particle.scale         = pow( system.sizeVariation, randomFloat( rng ) * 2.0 - 1.0 ); // Log-uniform: as many below the size of the system as above it
    particle.orientation   = randomOrientation( rng );
    particle.behaviorState = float4( 0.0 );

    switch ( system.behavior )
    {
        case PARTICLE_BEHAVIOR_ORBITALS:
            seedOrbital( system, rng, particle );
            break;
        default:
            break;
    }
    return particle;
}

void updateParticle( ParticleSystemData system, inout ParticleData particle ) {
    switch ( system.behavior )
    {
        case PARTICLE_BEHAVIOR_ORBITALS:
            updateOrbital( system, particle );
            break;
        default:
            break;
    }
}

// -----------------------------------------------------------
// CULLING
// -----------------------------------------------------------
bool isParticleVisible( ParticleSystemData system, ParticleData particle ) {
    MeshData mesh = getMeshData( system.meshID );

    // Sphere around the particle that holds the mesh with any orientation: no need of a matrix per particle
    const float radius = ( length( mesh.bsphere.xyz ) + mesh.bsphere.w ) * system.getSize() * particle.scale;
    float3      center = mul( system.modelMatrix, float4( particle.position, 1.0 ) ).xyz;
    return isSphereInFrustum( center, radius * getMaxScale( system.modelMatrix ) );
}

// -----------------------------------------------------------
[shader( "compute" )]
[numthreads( PARTICLE_GROUP_SIZE, 1, 1 )]
void csUpdateParticleSystem( uint3 dispatchThreadID: SV_DispatchThreadID ) {
    ParticleSystemData system        = gParticleSystems[gPush.systemIndex];
    uint               particleIndex = dispatchThreadID.x;
    if ( particleIndex >= system.count )
        return;

    RWStructuredBuffer<ParticleData> particles = getRWStructuredBuffer<ParticleData>( system.bufferUAVHeapID );

    // Simulation
    ParticleData particle;
    if ( system.reset != 0 )
    {
        particle                 = seedParticle( system, particleIndex );
        particles[particleIndex] = particle;
    } else
    {
        particle = particles[particleIndex];
        if ( system.update != 0 )
        {
            updateParticle( system, particle );
            particles[particleIndex] = particle;
        }
    }

    // Culling: every wave reserves its range of the visible list once, a single atomic per particle would contend a lot
    const bool visible       = system.visible != 0 && isParticleVisible( system, particle );
    const uint visibleInWave = WaveActiveCountBits( visible );

    uint waveOffset = 0;
    if ( WaveIsFirstLane() && visibleInWave > 0 )
        InterlockedAdd( gVisibleCounts[gPush.systemIndex], visibleInWave, waveOffset );
    waveOffset = WaveReadLaneFirst( waveOffset );

    if ( visible )
    {
        RWStructuredBuffer<uint> visibleList                = getRWStructuredBuffer<uint>( system.visibleListUAVHeapID );
        visibleList[waveOffset + WavePrefixCountBits( visible )] = particleIndex;
    }
}
