#ifdef __D3D12__
    // DX12: Wrapper + Control Manual
    #define PUSH_CONSTANT(Type, Name, Reg, Space) \
        ConstantBuffer<Type> Name : register(Reg, Space)
#else
    // VULKAN: Keyword nativa
    #define PUSH_CONSTANT(Type, Name, Reg, Space) \
        [[vk::push_constant]] \
        Type Name
#endif

public static const uint CANON_VERTEX_STRIDE = 64;

public struct DrawIndexedIndirectCommand {
    public uint baseInstanceID; ///< Current Instance ID + offset

    public uint indexCount;    ///< Number of indexes to draw
    public uint instanceCount; ///< Number of instances to draw
    public uint firstIndex;    ///< Offset in IndexBuffer (elements, not bytes)
    public int  vertexOffset;  ///< Offset in VertexBuffer
    public uint firstInstance; ///< ID as base

    uint _padding[2];
};