// BxDFs/IBxDF.slang

// Interface for any Bidirectional Distribution Function 
// (Lambert, Cook-Torrance, Glass, Hair...)
interface IBxDF
{
    // 1. EVAL (Raster & PT)
    float3 eval(float3 L, float3 V, float3 N);

    // 2. SAMPLE (Only PT)
    void sample(float3 V, float3 N, float3 rnd, out float3 L, out float pdf, out float3 weight);

    // 3. PROBABILITY  (Only PT)
    float pdf(float3 L, float3 V, float3 N);
};