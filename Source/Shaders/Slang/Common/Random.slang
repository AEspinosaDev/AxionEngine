// -----------------------------------------------------------------
//Simple Random Functions
// -----------------------------------------------------------------
uint nextRandom(inout uint state) {
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}
float randomFloat(inout uint state) { return nextRandom(state) / 4294967295.0; }

// -----------------------------------------------------------------
// Advanced Random Functions
// -----------------------------------------------------------------
float2 hammersley( uint index, uint numSamples, uint2 random )
{
    float E1 = frac( (float)index / numSamples + float( random.x & 0xffff ) / ( 1 << 16 ) );
    float E2 = float( reversebits( index ) ^ random.y ) * 2.3283064365386963e-10;
    return float2( E1, E2 );
}

float2 hammersley16( uint index, uint numSamples, uint2 random )
{
    float E1 = frac( (float)index / numSamples + float( random.x ) * ( 1.0 / 65536.0 ) );
    float E2 = float( ( reversebits( index ) >> 16 ) ^ random.y ) * ( 1.0 / 65536.0 );
    return float2( E1, E2 );
}

// http://extremelearning.com.au/a-simple-method-to-construct-isotropic-quasirandom-blue-noise-point-sequences/
float2 R2Sequence( uint index )
{
    const float  Phi = 1.324717957244746;
    const float2 a   = float2( 1.0 / Phi, 1.0 / pow( Phi, 2.0 ) );
    return frac( a * index );
}


