#pragma once

#include "Axion/Graphics/RHI/CommandList.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( DX12CommandList, DX12CommandList )

class DX12CommandList : public RefCounter<ICommandList>
{
public:
    DX12CommandList( const ComPtr<ID3D12Device2>& device, const CommandListDesc& desc );
    ~DX12CommandList() override;

    void                   begin() override;
    void                   end() override;
    void                   setCurrentFrame( uint index ) override;
    uint                   getCurrentFrame() const override;
    const CommandListDesc& getDescription() const override;

    void barrier( ITexture* texture, ResourceState newState ) override;
    void barrier( IBuffer* buffer, ResourceState newState ) override;
    void clearTexture( ITexture* texture, const ClearValue& clearValue ) override;
    void copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset = 0, ulong srcOffset = 0 ) override;
    void copyTexture( ITexture* dst, ITexture* src ) override;

    void bindComputePipeline( IComputePipeline* pipeline ) override;
    void bindGraphicPipeline( IGraphicPipeline* pipeline ) override;
    void bindDescriptorSet( uint setIndex, IDescriptorSet* set ) override;

    void dispatch( const Extent3D& gridSize ) override;

    void beginRendering( const RenderingDesc& info ) override;
    void endRendering() override;
    void draw( uint vertexCount, uint instanceCount = 1, uint firstVertex = 0, uint firstInstance = 0 ) override;
    void drawIndexed( uint indexCount,
                      uint instanceCount = 1,
                      uint firstIndex    = 0,
                      int  vertexOffset  = 0,
                      uint firstInstance = 0 ) override;
    void bindVertexBuffer( uint slot, IBuffer* buffer ) override;
    void bindIndexBuffer( IBuffer* buffer ) override;

    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    std::string        toString() const override;

private:
    void pushConstants( uint setIndex, const void* data, uint numValues32Bit, uint offset32Bit = 0 ) override;

    ComPtr<ID3D12GraphicsCommandList>           _cmdList;
    std::vector<ComPtr<ID3D12CommandAllocator>> _cmdAllocators;

    uint            _currentFrame = 0;
    CommandListDesc _desc;

    PipelineBindPoint     _bindPoint   = PipelineBindPoint::None;
    ID3D12DescriptorHeap* _currentHeap = nullptr;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END