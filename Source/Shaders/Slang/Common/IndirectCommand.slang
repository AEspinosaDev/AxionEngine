
// Indirect Rendering Commands
public struct DrawIndexedIndirectCommand {
    public uint baseInstanceID; ///< Current Instance ID + offset

    public uint indexCount;    ///< Number of indexes to draw
    public uint instanceCount; ///< Number of instances to draw
    public uint firstIndex;    ///< Offset in IndexBuffer (elements, not bytes)
    public int  vertexOffset;  ///< Offset in VertexBuffer
    public uint firstInstance; ///< ID as base

    uint _padding[2];
};

// -----------------------------------------------------------------