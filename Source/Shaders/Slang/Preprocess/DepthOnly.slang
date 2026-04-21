// DepthOnly.slang
#include "MaterialCommon.slang"


struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
};

[shader( "vertex" )]
VSOutput vsDepth(VSInput input) 
{
    InstanceData instance = getInstanceData(input.instanceID);
    MeshData mesh = getMeshData(instance.meshID);

    uint vertexAddress = mesh.vertexOffset + (input.vertexID * CANON_VERTEX_STRIDE);

    float3 localPos = asfloat(gGlobalVertices.Load3(vertexAddress));
    float2 uv = asfloat(gGlobalVertices.Load2(vertexAddress + 24));

    VSOutput output;
    float4 worldPos = mul(instance.modelMatrix, float4(localPos, 1.0));
    output.position = mul(gFrame.viewProj, worldPos);

    return output;
}
