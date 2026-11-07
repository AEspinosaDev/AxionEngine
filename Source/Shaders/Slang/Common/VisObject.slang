// MODULE: owners of the visibility buffer pixels. import it.
// The mesh shaders and the resolve place the vertices through them, so both see the same triangles by construction.
import MaterialCommon;

public interface IVisObject
{
    uint getMeshID();
    uint getMaterialID();

    // Mesh vertex -> world
    float4 placePosition( float3 position );
    /// @brief Mesh vertex -> world in the previous frame (velocity)
    float4 placePrevPosition( float3 position );
    float3 placeNormal( float3 normal );
};

public struct VisInstance : IVisObject {
    InstanceData instance;

    public __init( uint instanceID ) {
        instance = getInstanceData( instanceID );
    }

    public uint getMeshID() { return instance.meshID; }
    public uint getMaterialID() { return instance.materialID; }

    public float4 placePosition( float3 position ) {
        return mul( instance.modelMatrix, float4( position, 1.0 ) );
    }
    public float4 placePrevPosition( float3 position ) {
        return mul( instance.prevModelMatrix, float4( position, 1.0 ) );
    }
    public float3 placeNormal( float3 normal ) {
        return mul( (float3x3)instance.modelMatrix, normal );
    }
};

// There is no matrix per particle: the model matrix of the system moves all of them
public struct VisParticle : IVisObject {
    ParticleSystemData system;
    ParticleData       particle;

    public __init( uint systemIndex, uint particleIndex ) {
        system = gParticleSystems[systemIndex];

        StructuredBuffer<ParticleData> particles = getStructuredBuffer<ParticleData>( system.bufferSRVHeapID );
        particle                                 = particles[particleIndex];
    }

    // From the second word of the visibility buffer
    public __init( uint packed ) {
        uint systemIndex, particleIndex;
        unpackVisibilityParticle( packed, systemIndex, particleIndex );
        this = VisParticle( systemIndex, particleIndex );
    }

    public uint getMeshID() { return system.meshID; }
    public uint getMaterialID() { return system.materialID; }

    public float4 placePosition( float3 position ) {
        return mul( system.modelMatrix, float4( particle.position + rotate( position * system.getSize() * particle.scale ), 1.0 ) );
    }
    /// @brief TODO: no previous state of the particles or of the system yet: camera motion only
    public float4 placePrevPosition( float3 position ) {
        return placePosition( position );
    }
    public float3 placeNormal( float3 normal ) {
        return mul( (float3x3)system.modelMatrix, rotate( normal ) );
    }

    // By the unit quaternion of the particle
    float3 rotate( float3 v ) {
        const float3 q = particle.orientation.xyz;
        return v + 2.0 * cross( q, cross( q, v ) + particle.orientation.w * v );
    }
};

public bool isVisParticle( uint instanceID ) {
    return ( instanceID & VIS_PARTICLE_FLAG ) != 0;
}
