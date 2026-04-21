// Materials/StandardPBR.slang
#include "MaterialCommon.slang"
#include "CookTorranceBRDF.slang"

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

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4               position : SV_Position;
    float2               uv : TEXCOORD0;
    float3               normal : NORMAL;
    float3               worldPos : TEXCOORD1;
    nointerpolation uint materialID : DATA0;
};

[shader( "vertex" )]
VSOutput vsForward( VSInput input ) {
    InstanceData instance = getInstanceData( input.instanceID );
    MeshData     mesh     = getMeshData( instance.meshID );

    // Vertex fetch
    CanonVertex v        = CanonVertex.fromIndex( input.vertexID, mesh.vertexOffset );
    float3      localPos = v.position;
    float3      localNrm = v.normal;
    float2      uv       = v.uv;

    float4 worldPos = mul( instance.modelMatrix, float4( localPos, 1.0 ) );
    float3 worldNrm = normalize( mul( (float3x3)instance.modelMatrix, localNrm ) );

    VSOutput output;
    output.position   = mul( gFrame.viewProj, worldPos );
    output.worldPos   = worldPos.xyz;
    output.normal     = worldNrm;
    output.uv         = uv;
    output.materialID = instance.materialID;

    return output;
}

[shader( "fragment" )]
float4 psForward( VSOutput input )
    : SV_Target {

    // Load Material
    MaterialData       materialHeader = getMaterialData( input.materialID );
    StandardPBRPayload mat            = StandardPBRPayload.load( materialHeader.bufferOffset );

    float3 albedo    = mat.albedoFactor;
    float  roughness = mat.roughnessFactor;
    float  metallic  = mat.metallicFactor;

    //Texture Fetch
    SamplerState defaultSampler = gSamplers[0];
    const uint INVALID_TEX = 0xFFFFFFFF;
    if ( mat.albedoTexIndex != INVALID_TEX )
    {
        float4 albedoSample = gTextures[mat.albedoTexIndex].Sample( defaultSampler, input.uv );
        albedo = albedoSample.rgb;
    }

    // 3. Initialize BSDF
    float3 viewDir = gFrame.getCameraPos() - input.worldPos;

    CookTorranceBRDF brdf = CookTorranceBRDF.create( albedo, roughness, metallic );

    // Light Loop
    float3 lo = float3( 0.0 );

    float3 V = normalize( viewDir );
    float3 N = normalize( input.normal );

    uint lightCount = gFrame.getLightCount();
    for ( uint i = 0; i < lightCount; ++i )
    {
        LightData light = gLights[i];

        if ( !light.isActive )
            continue;

        float3 L        = 0.0f;
        float3 radiance = evalLight( light, input.worldPos, L );

        if ( any( radiance > 0.0001 ) )
        {
            lo += brdf.eval( L, V, N ) * radiance;
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
        float3 ambientDiffuse = kD_ambient * albedo * envIrradiance * mat.aoStrength;

        float3 ambientSpecular = float3( 0.0 );

        if ( metallic > 0.01 )
        {
            float reflectionDim = 1.0 / ( 1.0 + roughness * 8.0 );

            ambientSpecular = env.skyColor * env.groundColor * albedo * reflectionDim;
        }
        ambient += ambientDiffuse + ambientSpecular;
    }

    float3 color = lo + ambient + mat.emissiveFactor;

    return float4( color, 1.0 );
}
