// MODULE: bindless access to the device global heaps (no shader parameters). import it.

// DescriptorHandle<T> is Slang's portable bindless handle (uint2): on D3D12 it lowers to ResourceDescriptorHeap[x] /
// SamplerDescriptorHeap[x], on Vulkan to descriptor indexing. nonuniform(): indices may diverge across a wave (resolve).
public Texture2D getTexture2D( uint idx ) {
    Texture2D.Handle h = Texture2D.Handle( uint2( idx, 0 ) );
    return *nonuniform( h );
}
public Texture3D getTexture3D( uint idx ) {
    Texture3D.Handle h = Texture3D.Handle( uint2( idx, 0 ) );
    return *nonuniform( h );
}
public TextureCube getTextureCube( uint idx ) {
    TextureCube.Handle h = TextureCube.Handle( uint2( idx, 0 ) );
    return *nonuniform( h );
}
public SamplerState getSampler( uint idx ) {
    SamplerState.Handle h = SamplerState.Handle( uint2( idx, 0 ) );
    return *nonuniform( h );
}
