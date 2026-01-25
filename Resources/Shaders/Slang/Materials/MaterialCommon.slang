// MaterialCommon.slang
#include "Defines.slang"
#include "Scene.slang" 

// -----------------------------------------------------------
// GLOBAL RESOURCES (Bindless Contract)
// -----------------------------------------------------------

// Space 0: Persistent (Mega Buffers)
public ByteAddressBuffer gGlobalVertices  : register( t0, space0 );
public ByteAddressBuffer gGlobalIndices   : register( t1, space0 );
public ByteAddressBuffer gGlobalMaterials : register( t2, space0 );

// Space 1: Transient Hot (Per-Frame)
public ConstantBuffer<FrameData>      gFrame     : register( b0, space1 );
public StructuredBuffer<MeshData>     gMeshes    : register( t0, space1 );
public StructuredBuffer<MaterialData> gMaterials : register( t1, space1 );
public StructuredBuffer<InstanceData> gInstances : register( t2, space1 );
public StructuredBuffer<LightData>    gLights    : register( t3, space1 );

// Space 2: Push Constants
public struct PushConstant {
    public uint instanceID;
};

PUSH_CONSTANT( PushConstant, gPush, b0, space2 );

// -----------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------

// Función helper para obtener los datos de la instancia actual
public InstanceData getInstanceData() {
    return gInstances[gPush.instanceID];
}

public MeshData getMeshData(uint meshID) {
    return gMeshes[meshID];
}

public MaterialData getMaterialData(uint materialID) {
    return gMaterials[materialID];
}