// Materials/Phong.slang
import MaterialCommon;
import IMaterial;

// Must match PhongMaterial::GPUPayload
struct PhongPayload {
    // Chunk 0
    float3 diffuse;
    float  shininess;

    // Chunk 1
    float3 specular;
    uint   diffuseTexIndex;

    static PhongPayload load( uint bufferOffset ) {
        PhongPayload p;

        ByteAddressBuffer materials = getMaterialBuffer();
        float4            c0        = asfloat( materials.Load4( bufferOffset ) );
        uint4             c1        = materials.Load4( bufferOffset + 16 );

        p.diffuse         = c0.xyz;
        p.shininess       = c0.w;
        p.specular        = asfloat( c1.xyz );
        p.diffuseTexIndex = c1.w;

        return p;
    }
};

/// @brief Classic Blinn-Phong: lambert diffuse plus a normalized specular lobe, in the same light units as the PBR (evalLight)
public struct PhongMaterial : IMaterial {
    PhongPayload payload;

    [mutating]
    public void init( uint materialID ) {
        MaterialData header = getMaterialData( materialID );
        payload             = PhongPayload.load( header.bufferOffset );
    }

    public float3 computeWPO( SurfaceData surface ) {
        return float3( 0.0 );
    }

    float3 getDiffuse( SurfaceData surface ) {
        if ( payload.diffuseTexIndex == INVALID_TEX_ID )
            return payload.diffuse;

        // TODO: SampleGrad with analytic UV derivatives (no implicit derivatives in the compute resolve)
        return payload.diffuse * getMaterialTexture2D( payload.diffuseTexIndex ).SampleLevel( getDefaultSampler(), surface.uv, 0 ).rgb;
    }

    public float3 evalColor( SurfaceData surface ) {
        const float3 diffuse = getDiffuse( surface );
        const float3 N       = normalize( surface.shNormal );
        const float3 V       = normalize( surface.V );

        // Energy normalized lobe: the highlight keeps its energy when the glossiness changes
        const float specularNorm = ( payload.shininess + 8.0 ) / ( 8.0 * PI );

        float3 color = float3( 0.0 );

        uint lightCount = gFrame.getLightCount();
        for ( uint i = 0; i < lightCount; ++i )
        {
            LightData light = gLights[i];
            if ( !light.isActive )
                continue;

            float3 L        = float3( 0.0 );
            float3 radiance = evalLight( light, surface.worldPos, L );
            float  NdotL    = saturate( dot( N, L ) );
            if ( NdotL <= 0.0 )
                continue;

            float3 H        = normalize( L + V );
            float  specular = specularNorm * pow( saturate( dot( N, H ) ), payload.shininess );

            color += ( diffuse * INV_PI + payload.specular * specular ) * radiance * NdotL;
        }

        // Ambient
        uint envCount = gFrame.getEnvCount();
        for ( uint i = 0; i < envCount; ++i )
            color += diffuse * evalEnviroment( gEnvs[i], N );

        return color;
    }

    public float evalOpacity( SurfaceData surface ) {
        if ( payload.diffuseTexIndex != INVALID_TEX_ID )
            return getMaterialTexture2D( payload.diffuseTexIndex ).Sample( getDefaultSampler(), surface.uv ).a; // Pixel shader: implicit derivatives OK
        return 1.0;
    }

    public float3 sampleDirection( SurfaceData surface, float2 rnd, out float3 L, out float pdf ) {
        // TBD
        L   = surface.shNormal;
        pdf = 0.0;
        return float3( 0.0 );
    }
    public float evalPdf( SurfaceData surface, float3 L ) {
        // TBD
        return 0.0;
    }
};
