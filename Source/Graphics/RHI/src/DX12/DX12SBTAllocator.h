#pragma once
#include "Axion/Graphics/RHI/ShaderBindingTable.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12SBTAllocator : public RefCounter<ISBTAllocator>
{
public:
    DX12SBTAllocator( ID3D12Device* device, const SBTAllocatorDesc& desc );
    ~DX12SBTAllocator();

    SBT::BufferView allocate( const ShaderBindingTable& sbt, IRayTracingPipeline* pip ) override;
    void            reset() override;

    const Description& getDescription() const override { return _desc; }
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

private:
    SBTAllocatorDesc _desc;

    ComPtr<ID3D12Resource>    _buffer;
    uchar*                    _cpuBaseAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS _gpuBaseAddress = 0;

    ulong _currentOffset = 0;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END