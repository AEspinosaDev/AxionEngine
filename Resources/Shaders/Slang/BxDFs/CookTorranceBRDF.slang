// BxDFs/CookTorrance.slang
#include "BxDF.slang"
#include "BxDF_Math.slang"

// ------------------------------------------------------------------
// BSDF STRUCT
// ------------------------------------------------------------------
struct CookTorranceBRDF : IBxDF {
    float3 albedo;
    float  roughness;
    float  metallic;
    float3 F0;

    static CookTorranceBRDF create( float3 col, float r, float m ) {
        CookTorranceBRDF brdf;

        brdf.albedo    = col;
        brdf.roughness = clamp( r, 0.04, 1.0 );
        brdf.metallic  = clamp( m, 0.0, 1.0 );

        brdf.F0 = lerp( float3( 0.04 ), col, m );
        return brdf;
    }

    float3 eval( float3 L, float3 V, float3 N ) {

        float3 H = normalize( L + V );

        float NdotV = max( dot( N, V ), 0.0 );
        float NdotL = max( dot( N, L ), 0.0 );
        float NdotH = max( dot( N, H ), 0.0 );
        float HdotV = max( dot( H, V ), 0.0 );

        if ( NdotL <= 0.0 )
            return float3( 0.0 );

        float3 specularLobe = 0;
        float3 diffuseLobe  = 0;

        // 1. Cook-Torrance Specular
        float  D = D_GGX( NdotH, this.roughness );
        float  G = G_Smith( NdotV, NdotL, this.roughness );
        float3 F = F_Schlick( HdotV, this.F0 );

        float3 numerator = D * G * F;
        float  denom     = 4.0 * NdotV * NdotL + 0.0001;
        specularLobe = numerator / denom;

        // 2. Diffuse (Lambert)
        float3 kS = F;
        float3 kD = float3( 1.0 ) - kS;
        kD *= 1.0 - this.metallic;
        diffuseLobe = kD * this.albedo / PI;

        return ( diffuseLobe + specularLobe ) * NdotL;
    }

    // TBD ...

    void sample( float3 V, float3 N, float3 rnd, out float3 L, out float pdf, out float3 weight ) {
        float probSpecular = this.metallic; // Simplificación

        if ( rnd.z < probSpecular )
        {
            // L = SampleGGX(rnd.xy, roughness, N, V);
        } else
        {
            // L = CosineSampleHemisphere(rnd.xy, N);
        }

        pdf        = this.pdf( L, V, N );
        float3 val = this.eval( L, V, N );

        weight = ( pdf > 0.0 ) ? val / pdf : float3( 0.0 );
    }

    float pdf( float3 L, float3 V, float3 N ) {
        // return lerp(PdfCosine(L), PdfGGX(L), probSpecular);
        return 1.0; // Placeholder
    }
};
