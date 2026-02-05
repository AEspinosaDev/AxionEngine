// MaterialCommon.slang
#include "Defines.slang"
#include "Scene.slang"

// -----------------------------------------------------------
// GLOBAL RESOURCES (Bindless Contract)
// -----------------------------------------------------------

// Space 0: Persistent (Mega Buffers)
public ByteAddressBuffer gGlobalVertices : register( t0, space0 );
public ByteAddressBuffer gGlobalIndices : register( t1, space0 );
public ByteAddressBuffer gGlobalMaterials : register( t2, space0 );

// Space 1: Transient Hot (Per-Frame)
public ConstantBuffer<FrameData>      gFrame : register( b0, space1 );
public StructuredBuffer<MeshData>     gMeshes : register( t0, space1 );
public StructuredBuffer<MaterialData> gMaterials : register( t1, space1 );
public StructuredBuffer<InstanceData> gInstances : register( t2, space1 );
public StructuredBuffer<LightData>    gLights : register( t3, space1 );
public StructuredBuffer<uint>         gInstanceRedirection : register( t4, space1 );

// Space 2: Push Constants
public struct PushConstant {
    public uint baseInstanceID; // Software Instance ID (Usually set by the renderer)
};

PUSH_CONSTANT( PushConstant, gPush, b0, space2 );

// -----------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------

public InstanceData getInstanceData( uint hwInstanceID ) {
    return gInstances[gInstanceRedirection[gPush.baseInstanceID + hwInstanceID]];
}

public MeshData getMeshData( uint meshID ) {
    return gMeshes[meshID];
}

public MaterialData getMaterialData( uint materialID ) {
    return gMaterials[materialID];
}

// -----------------------------------------------------------
// CANONICAL VERTEX
// -----------------------------------------------------------

struct CanonVertex
{
    uint address;

    static CanonVertex fromIndex( uint vertexID, uint offsetInVertices )
    {
        CanonVertex v;
        v.address = offsetInVertices + ( vertexID * CANON_VERTEX_STRIDE );;
        return v;
    }

    property float3 position 
    {
        get { return asfloat( gGlobalVertices.Load3( address ) ); }
    }

    property float3 normal 
    {
        get { return asfloat( gGlobalVertices.Load3( address + 12 ) ); }
    }

    property float2 uv 
    {
        get { return asfloat( gGlobalVertices.Load2( address + 24 ) ); }
    }

    property float4 tangent 
    {
        get { return asfloat( gGlobalVertices.Load4( address + 32 ) ); }
    }

    property float4 color 
    {
        get { return asfloat( gGlobalVertices.Load4( address + 48 ) ); }
    }
};