// Materials/IMaterial.slang

public struct SurfaceData {
    public float3 worldPos = float3( 0.0 );
    public float3 shNormal = float3( 0.0, 1.0, 0.0 );
    public float2 uv       = float2( 0.0 );
    public float3 V        = float3( 0.0 );
};

public interface IMaterial
{
    [mutating]
    void init( uint materialID );

    // World Position Offset (for effects like parallax mapping, or vertex shader WPO)
    float3 computeWPO( SurfaceData surface );

    // Shading evaluations
    float3 evalColor( SurfaceData surface );
    float  evalOpacity( SurfaceData surface );

    // Path tracing
    float3 sampleDirection( SurfaceData surface, float2 rnd, out float3 L, out float pdf );
    float  evalPdf( SurfaceData surface, float3 L );
};
