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
    const CommandListDesc& getDescription() const override;

    void barrier( ITexture* texture, ResourceState newState ) override;
    void barrier( IBuffer* buffer, ResourceState newState ) override;
    void clearTexture( ITexture* texture, const ClearValue& clearValue ) override;
    void copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset = 0, ulong srcOffset = 0 ) override;

    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    std::string        toString() const override;

private:
    ComPtr<ID3D12GraphicsCommandList>           _cmdList;
    std::vector<ComPtr<ID3D12CommandAllocator>> _cmdAllocators;

    uint            _currentFrame = 0;
    CommandListDesc _desc;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END