
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

public struct DispatchIndirectCommand {
    public uint baseInstanceID; ///< Root constant (first push constant field)
    public uint threadGroupCountX;
    public uint threadGroupCountY;
    public uint threadGroupCountZ;
};

public struct DispatchMeshIndirectCommand {
    public uint baseInstanceID;    ///< Root constant (first push constant field)
    public uint threadGroupCountX; ///< Meshlets of the mesh
    public uint threadGroupCountY; ///< Instances (visible ones when GPU culling: atomically filled)
    public uint threadGroupCountZ;
};

// -----------------------------------------------------------------