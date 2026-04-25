// MaterialCommon.slang
#include "GlobalLayouts.slang"

// -----------------------------------------------------------
// GLOBAL DEFINITIONS
// -----------------------------------------------------------

public static const uint INVALID_TEX_ID  = 0xFFFFFFFF;
public static const uint INVALID_SAMPLER = 0xFFFFFFFF;

// -----------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------

public uint getRedirectedInstanceID( uint hwInstanceID ) {
    return gInstanceRedirection[gPush.baseInstanceID + hwInstanceID];
}

public InstanceData getInstanceData( uint instanceID ) {
    return gInstances[instanceID];
}

public MeshData getMeshData( uint meshID ) {
    return gMeshes[meshID];
}

public MaterialData getMaterialData( uint materialID ) {
    return gMaterials[materialID];
}

// -----------------------------------------------------------
// CANONICAL VERTEX
// -----------------------------------------------------------

struct CanonVertex {
    uint address;

    static CanonVertex fromIndex( uint vertexID, uint offsetInVertices ) {
        CanonVertex v;
        v.address = offsetInVertices + ( vertexID * CANON_VERTEX_STRIDE );
        ;
        return v;
    }

    property float3 position {
        get { return asfloat( gGlobalVertices.Load3( address ) ); }
    }

    property float3 normal {
        get { return asfloat( gGlobalVertices.Load3( address + 12 ) ); }
    }

    property float2 uv {
        get { return asfloat( gGlobalVertices.Load2( address + 24 ) ); }
    }

    property float4 tangent {
        get { return asfloat( gGlobalVertices.Load4( address + 32 ) ); }
    }

    property float4 color {
        get { return asfloat( gGlobalVertices.Load4( address + 48 ) ); }
    }
};

// -----------------------------------------------------------
// LIGHT EVALUATION
// -----------------------------------------------------------

public float3 evalLight( LightData light, float3 worldPos, out float3 outL ) {
    float attenuation = 1.0;

    if ( light.type == LIGHT_TYPE_DIR )
    {
        outL        = -normalize( light.direction );
        attenuation = 1.0;
    } else // Point & Spot
    {
        float3 lightVec = light.position - worldPos;
        float  dist     = length( lightVec );

        outL = lightVec / max( dist, 0.0001 );

        float range  = light.range;
        float factor = dist / range;

        if ( factor >= 1.0 )
            return float3( 0.0 );

        float smoothFactor = saturate( 1.0 - factor * factor * factor * factor );
        float distSq       = dist * dist;
        attenuation        = ( smoothFactor * smoothFactor ) / ( distSq + 1.0 );
    }

    // (Only Spot)
    if ( light.type == LIGHT_TYPE_SPOT )
    {
        float theta = dot( -outL, light.direction );

        float epsilon       = light.spotInnerCos - light.spotOuterCos;
        float spotIntensity = clamp( ( theta - light.spotOuterCos ) / epsilon, 0.0, 1.0 );

        attenuation *= spotIntensity;
    }

    // float shadow = 1.0;
    // if (light.castShadows) shadow = SampleShadow(light, worldPos);
    // attenuation *= shadow;

    return light.color * light.intensity * attenuation;
}

// -----------------------------------------------------------
// ENVIROMENT EVALUATION
// -----------------------------------------------------------

float calculateVolumeWeight( float3 worldPos, EnvData env ) {
    if ( env.procSettings.w < 0.5 )
        return 1.0;

    // TBD ....
    return 0.0;
}

public float3 evalEnviroment( EnvData env, float3 N ) {
    float3 envIrradiance = float3( 0.0 );

    float  hemiFactor   = 0.5 * N.y + 0.5;
    float3 ambientColor = lerp( env.groundColor, env.skyColor, hemiFactor );
    envIrradiance       = ambientColor * env.intensity;
    return envIrradiance;
    // switch ( env.type )
    // {
    //     case ENV_TYPE_CONST:

    //     case ENV_TYPE_PROC:
    //         return float3( 0.0 ); // TBD ...

    //     case ENV_TYPE_HDRI:
    //         return float3( 0.0 ); // TBD ...
    // }
}

