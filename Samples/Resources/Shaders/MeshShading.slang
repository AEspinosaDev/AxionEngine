// ==========================================================================================
// AXION ENGINE - MESH SHADING DRAW SHADER
// ==========================================================================================

struct CameraData {
    float4x4 viewModelProj;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float4 tangent;
    float4 color;
};

struct Meshlet {
    uint vertexOffset;
    uint vertexCount;
    uint triangleOffset;
    uint triangleCount;
    
    float3 center;
    float  radius;
    
    float3 coneApex;
    uint   packedCone; // 3 bytes for axis, 1 byte for cutoff
};


struct VertexOut {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};


ConstantBuffer<CameraData> ubo : register(b0);

Texture2D<float4> tex   : register(t0);
SamplerState      samp  : register(s0);

// Geometry Buffers
StructuredBuffer<Vertex>  vertices         : register(t1);
StructuredBuffer<Meshlet> meshlets         : register(t2);
StructuredBuffer<uint>    vertexIndices    : register(t3);
ByteAddressBuffer         primitiveIndices : register(t4); // uint8_t array

uint3 getPrimitiveIndices(uint triangleIndex, uint triangleOffset) {
    uint byteOffset = triangleOffset + (triangleIndex * 3);
    uint wordOffset = byteOffset & ~3; // Align down to nearest multiple of 4
    uint byteShift  = byteOffset & 3;  // Remainder (0, 1, 2, or 3)

    // Load two consecutive words to ensure we have all 3 bytes regardless of alignment
    uint word0 = primitiveIndices.Load(wordOffset);
    uint word1 = primitiveIndices.Load(wordOffset + 4);

    uint i0, i1, i2;

    if (byteShift == 0) {
        i0 = word0 & 0xFF;
        i1 = (word0 >> 8) & 0xFF;
        i2 = (word0 >> 16) & 0xFF;
    } else if (byteShift == 1) {
        i0 = (word0 >> 8) & 0xFF;
        i1 = (word0 >> 16) & 0xFF;
        i2 = (word0 >> 24) & 0xFF;
    } else if (byteShift == 2) {
        i0 = (word0 >> 16) & 0xFF;
        i1 = (word0 >> 24) & 0xFF;
        i2 = word1 & 0xFF; // Crosses over to the second word
    } else { // byteShift == 3
        i0 = (word0 >> 24) & 0xFF;
        i1 = word1 & 0xFF;
        i2 = (word1 >> 8) & 0xFF;
    }

    return uint3(i0, i1, i2);
}

// ------------------------------------------------------------------
// MESH SHADER STAGE
// ------------------------------------------------------------------
// We use 128 threads per group. This covers the maximum possible
// vertices (64) and maximum possible triangles (126) without looping.
[shader( "mesh" )]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void msMain(
    uint                          gtid: SV_GroupThreadID,
    uint                          gid: SV_GroupID,
    OutputIndices<uint3, 126>     tris,
    OutputVertices<VertexOut, 64> verts
) {
    // 1. Fetch the Meshlet metadata for this Thread Group
    Meshlet m = meshlets[gid];

    // 2. Allocate the exact amount of hardware space needed for this meshlet
    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    // 3. Thread Group Parallel Execution: Process Vertices
    // Only the first 'vertexCount' threads participate
    if (gtid < m.vertexCount) {
        uint globalVertexIndex = vertexIndices[m.vertexOffset + gtid];
        Vertex v = vertices[globalVertexIndex];

        VertexOut vout;
        vout.position = mul(ubo.viewModelProj, float4(v.position, 1.0));
        vout.uv       = v.uv;
        vout.color    = v.color;
        
        verts[gtid] = vout;
    }

    // 4. Thread Group Parallel Execution: Process Triangles
    // Only the first 'triangleCount' threads participate
    if (gtid < m.triangleCount) {
        // Read the local indices (0-63) that form this triangle
        uint3 localIndices = getPrimitiveIndices(gtid, m.triangleOffset);
        
        // Output the topology connectivity
        tris[gtid] = localIndices;
    }
}

// ------------------------------------------------------------------
// PIXEL SHADER STAGE
// ------------------------------------------------------------------
[shader( "fragment" )]
float4 psMain(VertexOut pin) : SV_Target {
    float4 texColor = tex.Sample(samp, pin.uv);
    return texColor;
}