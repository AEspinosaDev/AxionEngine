// Materials/StandardPBR.slang
#include "MaterialCommon.slang"
#include "CookTorranceBRDF.slang"
#include "IMaterial.slang"

struct StandardPBRPayload {
    // Chunk 0
    float3 albedoFactor;
    float  roughnessFactor;

    // Chunk 1
    float3 emissiveFactor;
    float  metallicFactor;

    // Chunk 2
    float normalScale;
    float aoStrength;
    uint  albedoTexIndex;
    uint  normalTexIndex;

    // Chunk 3
    uint armTexIndex;
    uint emissiveTexIndex;

    static StandardPBRPayload load( uint bufferOffset ) {
        StandardPBRPayload p;

        float4 c0 = asfloat( gGlobalMaterials.Load4( bufferOffset ) );
        float4 c1 = asfloat( gGlobalMaterials.Load4( bufferOffset + 16 ) );
        uint4  c2 = gGlobalMaterials.Load4( bufferOffset + 32 );
        uint4  c3 = gGlobalMaterials.Load4( bufferOffset + 48 );

        p.albedoFactor    = c0.xyz;
        p.roughnessFactor = c0.w;

        p.emissiveFactor = c1.xyz;
        p.metallicFactor = c1.w;

        p.normalScale    = asfloat( c2.x );
        p.aoStrength     = asfloat( c2.y );
        p.albedoTexIndex = c2.z;
        p.normalTexIndex = c2.w;

        p.armTexIndex      = c3.x;
        p.emissiveTexIndex = c3.y;

        return p;
    }
};

struct StandardPBRMaterial : IMaterial {
    StandardPBRPayload payload;

    [mutating]
    void init( uint materialID ) {
        MaterialData header = getMaterialData( materialID );
        payload             = StandardPBRPayload.load( header.bufferOffset );
    }

    float3 computeWPO( SurfaceData surface ) {
        return float3( 0.0 ); // TBD: Parallax Mapping
    }

    float3 evalColor( SurfaceData surface ) {

        float3 albedo    = payload.albedoFactor;
        float  roughness = payload.roughnessFactor;
        float  metallic  = payload.metallicFactor;

        // Texture Fetch
        SamplerState defaultSampler = gSamplers[0];

        if ( payload.albedoTexIndex != INVALID_TEX_ID )
        {
            float4 albedoSample = gTextures[payload.albedoTexIndex].Sample( defaultSampler, surface.uv );
            albedo              = albedoSample.rgb;
        }

        // Initialize BSDF
        float3           viewDir = gFrame.getCameraPos() - surface.worldPos;
        CookTorranceBRDF brdf    = CookTorranceBRDF.create( albedo, roughness, metallic );

        // Light Loop
        float3 lo = float3( 0.0 );

        float3 N = normalize( surface.shNormal );

        uint lightCount = gFrame.getLightCount();
        for ( uint i = 0; i < lightCount; ++i )
        {
            LightData light = gLights[i];

            if ( !light.isActive )
                continue;

            float3 L        = 0.0f;
            float3 radiance = evalLight( light, surface.worldPos, L );

            if ( any( radiance > 0.0001 ) )
            {
                lo += brdf.eval( L, surface.V, N ) * radiance;
            }
        }

        // Ambient / Emissive
        float3 ambient = float3( 0.0 );

        uint envCount = gFrame.getEnvCount();
        for ( uint i = 0; i < envCount; ++i )
        {
            EnvData env           = gEnvs[i];
            float3  envIrradiance = evalEnviroment( env, N );

            float3 kD_ambient     = ( 1.0 - metallic );
            float3 ambientDiffuse = kD_ambient * albedo * envIrradiance * payload.aoStrength;

            float3 ambientSpecular = float3( 0.0 );

            if ( metallic > 0.01 )
            {
                float reflectionDim = 1.0 / ( 1.0 + roughness * 8.0 );

                ambientSpecular = env.skyColor * env.groundColor * albedo * reflectionDim;
            }
            ambient += ambientDiffuse + ambientSpecular;
        }

        float3 color = lo + ambient + payload.emissiveFactor;

        return color;
    }
    float evalOpacity( SurfaceData surface ) {
        // Ejemplo simple: solo comprobamos alfa si hay textura
        if ( payload.albedoTexIndex != 0xFFFFFFFF )
        {
            return gTextures[payload.albedoTexIndex].Sample( gSamplers[0], surface.uv ).a;
        }
        return 1.0;
    }

    float3 sampleDirection( SurfaceData surface, float2 rnd, out float3 L, out float pdf ) {
        // TBD
    };
    float evalPdf( SurfaceData surface, float3 L ) {};
};
