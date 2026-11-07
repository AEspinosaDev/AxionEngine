// Materials/Unlit.slang
import MaterialCommon;
import IMaterial;

// Must match UnlitMaterial::GPUPayload
struct UnlitPayload {
    // Chunk 0
    float3 color;
    float  emissivePower;

    // Chunk 1
    uint  colorTexIndex;
    float overrideStrength;

    static UnlitPayload load( uint bufferOffset ) {
        UnlitPayload p;

        ByteAddressBuffer materials = getMaterialBuffer();
        float4            c0        = asfloat( materials.Load4( bufferOffset ) );
        uint4             c1        = materials.Load4( bufferOffset + 16 );

        p.color            = c0.xyz;
        p.emissivePower    = c0.w;
        p.colorTexIndex    = c1.x;
        p.overrideStrength = asfloat( c1.y );

        return p;
    }
};

/// @brief Flat color without lighting. A texture replaces the color by overrideStrength
public struct UnlitMaterial : IMaterial {
    UnlitPayload payload;

    [mutating]
    public void init( uint materialID ) {
        MaterialData header = getMaterialData( materialID );
        payload             = UnlitPayload.load( header.bufferOffset );
    }

    public float3 computeWPO( SurfaceData surface ) {
        return float3( 0.0 );
    }

    public float3 evalColor( SurfaceData surface ) {
        float3 color = payload.color;

        if ( payload.colorTexIndex != INVALID_TEX_ID )
        {
            // TODO: SampleGrad with analytic UV derivatives (no implicit derivatives in the compute resolve)
            float3 texColor = getMaterialTexture2D( payload.colorTexIndex ).SampleLevel( getDefaultSampler(), surface.uv, 0 ).rgb;
            color           = lerp( color, texColor, payload.overrideStrength );
        }

        return color * payload.emissivePower;
    }

    public float evalOpacity( SurfaceData surface ) {
        if ( payload.colorTexIndex != INVALID_TEX_ID )
            return getMaterialTexture2D( payload.colorTexIndex ).Sample( getDefaultSampler(), surface.uv ).a; // Pixel shader: implicit derivatives OK
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
