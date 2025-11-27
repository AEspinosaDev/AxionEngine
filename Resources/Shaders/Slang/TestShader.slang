StructuredBuffer<float> buffer0 : register(t0, space0);
StructuredBuffer<float> buffer1 : register(t1, space0);
RWStructuredBuffer<float> result : register(u0, space1);

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    result[index] = buffer0[index] + buffer1[index];
}