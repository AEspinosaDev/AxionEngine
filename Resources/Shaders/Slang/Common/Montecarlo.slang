

// -----------------------------------------------------------------
// Basis Operations
// -----------------------------------------------------------------

// [ Duff et al. 2017, "Building an Orthonormal Basis, Revisited" ]
// Discontinuity at TangentZ.z == 0
float3x3 getTangentBasis( float3 tangentZ )
{
    const float sign = tangentZ.z >= 0 ? 1 : -1;
    const float a    = -rcp( sign + tangentZ.z );
    const float b    = tangentZ.x * tangentZ.y * a;

    float3 tangentX = { 1 + sign * a * pow( tangentZ.x, 2.0 ), sign * b, -sign * tangentZ.x }; // <-- CORREGIDO
    float3 tangentY = { b, sign + a * pow( tangentZ.y, 2.0 ), -tangentZ.y };

    return float3x3( tangentX, tangentY, tangentZ );
}

// -----------------------------------------------------------------
// Warping
// -----------------------------------------------------------------
const static float PI = 3.14159265358979323846;

// PDF = 1
float4 uniformSampleSphere( float2 E ) {
    float phi      = 2 * PI * E.x;
    float cosTheta = 1 - 2 * E.y;
    float sinTheta = sqrt( 1 - cosTheta * cosTheta );

    float3 H;
    H.x = sinTheta * cos( phi );
    H.y = sinTheta * sin( phi );
    H.z = cosTheta;

    float PDF = 1.0 / ( 4 * PI );

    return float4( H, PDF );
}

// PDF = 1 / (2 * PI)
float4 uniformSampleHemisphere( float2 E ) {
    float phi      = 2 * PI * E.x;
    float cosTheta = E.y;
    float sinTheta = sqrt( 1 - cosTheta * cosTheta );

    float3 H;
    H.x = sinTheta * cos( phi );
    H.y = sinTheta * sin( phi );
    H.z = cosTheta;

    float PDF = 1.0 / ( 2 * PI );

    return float4( H, PDF );
}

// PDF = NoL / PI
float4 cosineSampleHemisphere( float2 E ) {
    float phi      = 2 * PI * E.x;
    float cosTheta = sqrt( E.y );
    float sinTheta = sqrt( 1 - cosTheta * cosTheta );

    float3 H;
    H.x = sinTheta * cos( phi );
    H.y = sinTheta * sin( phi );
    H.z = cosTheta;

    float PDF = cosTheta * ( 1.0 / PI );

    return float4( H, PDF );
}
