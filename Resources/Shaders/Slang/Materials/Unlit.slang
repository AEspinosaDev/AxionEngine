// Test.slang / Unlit.slang
#include "MaterialCommon.slang" 

struct UnlitMaterialPayload 
{
    float3 color;
    float  emissivePower;
    uint   textureIndex;
    float  overrideStrength;

    static UnlitMaterialPayload load(uint bufferOffset) 
    {
        UnlitMaterialPayload p;

        float4 chunk0 = asfloat(gGlobalMaterials.Load4(bufferOffset));
        float4 chunk1 = asfloat(gGlobalMaterials.Load4(bufferOffset + 16));

        p.color            = chunk0.xyz;
        p.emissivePower    = chunk0.w;
        
        uint4 rawChunk1 = gGlobalMaterials.Load4(bufferOffset + 16);
        p.textureIndex     = rawChunk1.x;
        p.overrideStrength = asfloat(rawChunk1.y); 

        return p;
    }
};

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
    float2               uv : TEXCOORD0;
    nointerpolation uint materialID : DATA0;
};

// -----------------------------------------------------------
// SHADERS
// -----------------------------------------------------------

[shader("vertex")]
VSOutput vsForward(VSInput input) 
{
    InstanceData instance = getInstanceData(input.instanceID);
    MeshData mesh = getMeshData(instance.meshID);

    uint vertexAddress = mesh.vertexOffset + (input.vertexID * CANON_VERTEX_STRIDE);

    float3 localPos = asfloat(gGlobalVertices.Load3(vertexAddress));
    float2 uv = asfloat(gGlobalVertices.Load2(vertexAddress + 24));

    VSOutput output;
    float4 worldPos = mul(instance.modelMatrix, float4(localPos, 1.0));
    output.position = mul(gFrame.viewProj, worldPos);
    output.uv         = uv;
    output.materialID = instance.materialID;

    return output;
}

[shader("fragment")]
float4 psForward(VSOutput input) : SV_Target 
{
    MaterialData materialHeader = getMaterialData(input.materialID);

    UnlitMaterialPayload mat = UnlitMaterialPayload.load(materialHeader.bufferOffset);

    float3 finalColor = mat.color;

    return float4(finalColor, 1.0);
}