#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Descriptor.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12DescriptorHeap
{
public:
    enum class Type
    {
        CBV_SRV_UAV,
        RTV,
        DSV
    };

    void init( ID3D12Device* device, Type type, uint numDescriptors, bool shaderVisible = false );
    void reset();

    D3D12_CPU_DESCRIPTOR_HANDLE allocateCPU();
    D3D12_GPU_DESCRIPTOR_HANDLE getGPU( D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle ) const;

    ID3D12DescriptorHeap* getHeap() const { return _heap.Get(); }
    Type                  getType() const { return _type; }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUStart() const { return _baseCPU; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUStart() const { return _baseGPU; }

    void setDebugName( const std::string& name );

private:
    ComPtr<ID3D12DescriptorHeap> _heap           = nullptr;
    uint                         _descriptorSize = 0;
    uint                         _allocated      = 0;
    uint                         _capacity       = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE  _baseCPU {};
    D3D12_GPU_DESCRIPTOR_HANDLE  _baseGPU {};
    Type                         _type;
};

class DX12DescriptorSet : public RefCounter<IDescriptorSet>
{
public:
    DX12DescriptorSet( ID3D12Device*               device,
                       ID3D12DescriptorHeap*       ownerHeap,
                       D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                       D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
                       uint                        descriptorSize );
    ~DX12DescriptorSet() override;

    void attach( uint binding, ITexture* tex, ResourceState bindingState ) override;
    void attach( uint binding, IBuffer* buf, ResourceState bindingState ) override;

    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const { return _startGPU; }
    ID3D12DescriptorHeap*       getOwnerHeap() const { return _ownerHeap; }

    void reconfigure( D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle ) {
        _startCPU = cpuHandle;
        _startGPU = gpuHandle;
    }

private:
    ID3D12Device*               _device;
    D3D12_CPU_DESCRIPTOR_HANDLE _startCPU;
    D3D12_GPU_DESCRIPTOR_HANDLE _startGPU;
    uint                        _handleSize;
    ID3D12DescriptorHeap*       _ownerHeap = nullptr;
};

class DX12DescriptorAllocator : public RefCounter<IDescriptorAllocator>
{
public:
    DX12DescriptorAllocator( ID3D12Device* device, const DescriptorAllocatorDesc& desc );
    ~DX12DescriptorAllocator() override;

    IDescriptorSet*                allocate( IPipelineLayout* layout, uint setIndex ) override;
    void                           reset() override;
    const DescriptorAllocatorDesc& getDescription() const override { return _desc; }

    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

private:
    ID3D12Device*           _device;
    DescriptorAllocatorDesc _desc;

    DX12DescriptorHeap _heap;

    uint _currentOffset = 0;
    uint _handleSize    = 0;

    // Pooling
    std::vector<std::unique_ptr<DX12DescriptorSet>> _setPool;
    uint                                            _poolIndex = 0;
};
} // namespace Graphics::RHI

AXION_NAMESPACE_END