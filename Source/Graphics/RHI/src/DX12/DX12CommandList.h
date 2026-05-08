#pragma once

#include "Axion/Graphics/RHI/ICommandList.h"
#include "DX12Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( DX12CommandList, DX12CommandList )

class DX12CommandList : public ICommandList
{
public:
    DX12CommandList( const ComPtr<ID3D12Device2>& device,
                     const CommandListDesc&       desc );
    ~DX12CommandList() override;

    void                   begin() override;
    void                   end() override;
    void                   setCurrentFrame( u32 index ) override;
    u32                   getCurrentFrame() const override;
    const CommandListDesc& getDescription() const override;

    void barrier( ITexture* texture, ResourceState newState ) override;
    void barrier( IBuffer* buffer, ResourceState newState ) override;
    void clearTexture( ITexture* texture, const ClearValue& clearValue, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) override;
    void copyBuffer( IBuffer* dst, IBuffer* src, u64 numBytes, u64 dstOffset = 0, u64 srcOffset = 0, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) override;
    void copyTexture( ITexture* dst, ITexture* src, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) override;

    void uploadBuffer( IBuffer* dst, const void* data, u64 size, u64 dstOffset, TransientDataAllocator& allocator, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) override;
    void uploadTexture( ITexture* dst, const void* data, TransientDataAllocator& allocator, u32 mipSlice = 0, u32 arraySlice = 0, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) override;
    void updateAccel( IAccel* accel, const AccelDesc& newDesc, TransientDataAllocator& allocator ) override;
    void buildAccel( IAccel* accel, const AccelDesc& newDesc, TransientDataAllocator& allocator ) override;

    void bindComputePipeline( IComputePipeline* pipeline ) override;
    void bindGraphicPipeline( IGraphicPipeline* pipeline ) override;
    void bindRaytracingPipeline( IRayTracingPipeline* pipeline ) override;
    void bindMeshPipeline( IMeshPipeline* pipeline ) override;

    void bindDescriptorSet( u32 setIndex, IDescriptorSet* set ) override;
    void bindDescriptorSet( u32 setIndex, IDescriptorSet* set, IPipelineLayout* layout, PipelineBindPoint bindPoint = PipelineBindPoint::Graphic ) override;

    void dispatch( const Extent3D& gridSize ) override;
    void dispatchMesh( const Extent3D& gridSize ) override;
    void dispatchRays( const SBT::Allocation& sbtView, const Extent3D& screenSize ) override;

    void beginRendering( const RenderingDesc& info ) override;
    void endRendering() override;
    void draw( u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0 ) override;
    void drawIndexed( u32 indexCount,
                      u32 instanceCount = 1,
                      u32 firstIndex    = 0,
                      int  vertexOffset  = 0,
                      u32 firstInstance = 0 ) override;
    void bindVertexBuffer( u32 slot, IBuffer* buffer ) override;
    void bindIndexBuffer( IBuffer* buffer ) override;

    void drawIndexedIndirect( IBuffer* indirectBuffer,
                              u64    bufferOffset,
                              u32     maxDrawCount,
                              IBuffer* countBuffer       = nullptr,
                              u64    countBufferOffset = 0 ) override;

    NativeObject getNativeObject( ObjectType objectType ) override;
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

private:
    void pushConstants( u32 setIndex, const void* data, u32 numValues32Bit, u32 offset32Bit = 0 ) override;
    bool validateUpdateCompatibility( const IAccel* accel, const AccelDesc& newDesc );

    ComPtr<ID3D12GraphicsCommandList>            _cmdList;
    SmallVector<ComPtr<ID3D12CommandAllocator>, 3> _cmdAllocators;

    u32            _currentFrame = 0;
    CommandListDesc _desc;

    PipelineBindPoint _bindPoint = PipelineBindPoint::None;

    ID3D12DescriptorHeap* _currentViewHeap    = nullptr;
    ID3D12DescriptorHeap* _currentSamplerHeap = nullptr;

    //TODO: Cache layout per bind point type (Compute + Graphics)
    IPipelineLayout* _currentLayout = nullptr;

    // To support RTX
    ID3D12GraphicsCommandList4* _cmdList4 = nullptr;
    // To support Mesh Shading
    ID3D12GraphicsCommandList6* _cmdList6 = nullptr;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END