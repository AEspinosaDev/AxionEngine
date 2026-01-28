#include "Defines.slang"
#include "Scene.slang"

// Space 0: Inputs
ConstantBuffer<FrameData>      gFrame : register( b0, space0 );
StructuredBuffer<MeshData>     gMeshes : register( t0, space0 );
StructuredBuffer<InstanceData> gInstances : register( t1, space0 );
StructuredBuffer<uint>         gInstanceRedirection : register( t2, space0 );
StructuredBuffer<uint>         gBatchMap : register( t3, space0 );

// Space 1: Outputs
RWStructuredBuffer<DrawIndexedIndirectCommand> gDrawCmds : register( u0, space1 );
RWStructuredBuffer<uint>                       gCulledRedirect : register( u1, space1 );

// Space 2: Push Constants
public struct PushConstant {
    public uint totalInstanceCount; 
};

PUSH_CONSTANT( PushConstant, gPush, b0, space2 );

[shader( "compute" )]
[numthreads( 64, 1, 1 )]
void computeMain( uint3 dispatchThreadId: SV_DispatchThreadID ) {
    uint tID = dispatchThreadId.x;

    if ( tID >= gPush.totalInstanceCount )
        return;

    uint         realEntityID = gInstanceRedirection[tID];
    InstanceData instance     = gInstances[realEntityID];

    if ( instance.active == 0 )
        return;

    MeshData mesh        = gMeshes[instance.meshID];
    float4   localCenter = float4( mesh.bsphere.xyz, 1.0 );
    float3   worldCenter = mul( instance.modelMatrix, localCenter ).xyz;

    float3 scale;
    scale.x           = length( instance.modelMatrix[0].xyz );
    scale.y           = length( instance.modelMatrix[1].xyz );
    scale.z           = length( instance.modelMatrix[2].xyz );
    float maxScale    = max( scale.x, max( scale.y, scale.z ) );
    float worldRadius = mesh.bsphere.w * maxScale;

    bool isVisible = true;

    [unroll]
    for ( int i = 0; i < 6; i++ )
    {
        float4 plane = gFrame.frustrum[i];
        if ( dot( float4( worldCenter, 1.0 ), plane ) < -worldRadius )
        {
            isVisible = false;
            break;
        }
    }

    if ( isVisible )
    {
        uint cmdIndex = gBatchMap[tID];

        uint mySlotIndex;
        InterlockedAdd( gDrawCmds[cmdIndex].instanceCount, 1, mySlotIndex );

        uint writePos = gDrawCmds[cmdIndex].baseInstanceID + mySlotIndex;

        gCulledRedirect[writePos] = realEntityID;
    }
}
