// MaterialCommon.slang
__exported import GlobalLayouts;
__exported import Constants;

// -----------------------------------------------------------
// GLOBAL DEFINITIONS
// -----------------------------------------------------------

public static const uint INVALID_TEX_ID  = 0xFFFFFFFF;
public static const uint INVALID_SAMPLER = 0xFFFFFFFF;

// -----------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------

// baseInstanceID comes from each pass push constant (first field, written by the indirect command signature)
public uint getRedirectedInstanceID( uint baseInstanceID, uint hwInstanceID ) {
    return gInstanceRedirection[baseInstanceID + hwInstanceID];
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

// Resolves a material texture slot (GPUScene cache slot stored in payloads) to its bindless texture.
// Falls back to the default texture while the real one is still being streamed.
public Texture2D getMaterialTexture2D( uint texSlot ) {
    uint heapID = gTextures[texSlot].heapID;
    return getTexture2D( heapID != INVALID_TEX_ID ? heapID : gFrame.fallbackTex2DID );
}

public SamplerState getDefaultSampler() {
    return getSampler( gFrame.linearSamplerID );
}

// -----------------------------------------------------------
// CANONICAL VERTEX
// -----------------------------------------------------------

public struct CanonVertex {
    public uint address;

    public static CanonVertex fromIndex( uint vertexID, uint offsetInVertices ) {
        CanonVertex v;
        v.address = offsetInVertices + ( vertexID * CANON_VERTEX_STRIDE );
        ;
        return v;
    }

    public property float3 position {
        get { return asfloat( gGlobalVertices.Load3( address ) ); }
    }

    public property float3 normal {
        get { return asfloat( gGlobalVertices.Load3( address + 12 ) ); }
    }

    public property float2 uv {
        get { return asfloat( gGlobalVertices.Load2( address + 24 ) ); }
    }

    public property float4 tangent {
        get { return asfloat( gGlobalVertices.Load4( address + 32 ) ); }
    }

    public property float4 color {
        get { return asfloat( gGlobalVertices.Load4( address + 48 ) ); }
    }
};

// -----------------------------------------------------------
// MESHLETS (cooked by meshoptimizer, see Assets::Meshlet)
// -----------------------------------------------------------

public struct Meshlet {
    public uint vertexOffset;   // Into the mesh meshlet vertices (elements)
    public uint vertexCount;
    public uint triangleOffset; // Into the mesh meshlet primitives (bytes)
    public uint triangleCount;

    public float3 center; // Culling bounds
    public float  radius;
    public float3 coneApex;
    public uint   packedCone; // s8 x3 axis | s8 cutoff
};

public Meshlet getMeshlet( MeshData mesh, uint meshletIndex ) {
    return gGlobalMeshlets.Load<Meshlet>( mesh.meshletOffset + meshletIndex * MESHLET_STRIDE );
}

// Mesh local vertex index (feeds CanonVertex.fromIndex)
public uint getMeshletVertexIndex( MeshData mesh, Meshlet meshlet, uint localVertex ) {
    return gGlobalMeshletVertices.Load( mesh.meshletVertexOffset + ( meshlet.vertexOffset + localVertex ) * 4 ); // u32 indices
}

// Meshlet local triangle. u8 x3 packed, not 4 B aligned: may straddle two words
public uint3 getMeshletTriangle( MeshData mesh, Meshlet meshlet, uint localTriangle ) {
    uint byteAddress = mesh.meshletPrimitiveOffset + meshlet.triangleOffset + localTriangle * 3;
    uint wordAddress = byteAddress & ~3u;
    uint shift       = ( byteAddress & 3u ) * 8;
    uint lo          = gGlobalMeshletPrimitives.Load( wordAddress );
    uint hi          = gGlobalMeshletPrimitives.Load( wordAddress + 4 );
    uint packed      = shift == 0 ? lo : ( lo >> shift ) | ( hi << ( 32 - shift ) );
    return uint3( packed & 0xFF, ( packed >> 8 ) & 0xFF, ( packed >> 16 ) & 0xFF );
}

public uint packVisibilityTriangle( uint meshletIndex, uint localTriangle ) {
    return ( meshletIndex << VIS_TRIANGLE_BITS ) | localTriangle;
}

public void unpackVisibilityTriangle( uint packed, out uint meshletIndex, out uint localTriangle ) {
    meshletIndex  = packed >> VIS_TRIANGLE_BITS;
    localTriangle = packed & VIS_TRIANGLE_MASK;
}

// -----------------------------------------------------------
// CULLING
// -----------------------------------------------------------

// Max axis scale of a model matrix (bounding sphere radius to world)
public float getMaxScale( float4x4 modelMatrix ) {
    return max( length( modelMatrix[0].xyz ), max( length( modelMatrix[1].xyz ), length( modelMatrix[2].xyz ) ) );
}

// World space sphere vs camera frustum
public bool isSphereInFrustum( float3 center, float radius ) {
    [unroll]
    for ( uint i = 0; i < 6; ++i )
    {
        if ( dot( float4( center, 1.0 ), gFrame.frustrumPlanes[i] ) < -radius )
            return false;
    }
    return true;
}

// Meshlet normal cone (meshoptimizer s8 SNORM, decode x / 127): xyz = axis, w = cutoff
public float4 unpackMeshletCone( uint packedCone ) {
    int4 c = int4( (int)( packedCone << 24 ) >> 24, (int)( packedCone << 16 ) >> 24, (int)( packedCone << 8 ) >> 24, (int)packedCone >> 24 );
    return float4( c ) / 127.0;
}

// Every triangle of the cluster faces away from the camera (perspective formula, meshoptimizer docs)
public bool isConeBackfacing( float3 apex, float3 axis, float cutoff, float3 cameraPos ) {
    return dot( normalize( apex - cameraPos ), axis ) >= cutoff;
}

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

