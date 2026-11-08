float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float luma( float3 c ) {
    return dot( c, float3( 0.2126, 0.7152, 0.0722 ) );
}

/// @brief Reversible tonemap (Karis): blending here weights every sample by 1 / (1 + luma), no flickering fireflies
float3 tonemap( float3 c ) {
    return c * rcp( 1.0 + luma( c ) );
}
float3 inverseTonemap( float3 c ) {
    return c * rcp( max( 1.0 - luma( c ), 1e-5 ) );
}

float3 rgbToYCoCg( float3 c ) {
    return float3( 0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                   0.5 * c.r - 0.5 * c.b,
                   -0.25 * c.r + 0.5 * c.g - 0.25 * c.b );
}
float3 yCoCgToRgb( float3 c ) {
    return float3( c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z );
}

/// @brief HDR -> working space (exposed, tonemapped, YCoCg)
float3 toWorking( float3 hdr ) {
    return rgbToYCoCg( tonemap( hdr * gPush.exposure ) );
}
/// @brief Working space -> HDR
float3 fromWorking( float3 w ) {
    return inverseTonemap( yCoCgToRgb( w ) ) / gPush.exposure;
}