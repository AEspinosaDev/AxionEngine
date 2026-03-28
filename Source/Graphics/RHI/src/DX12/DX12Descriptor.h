#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IDescriptor.h"
#include "DX12Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12DescriptorHeap
{
public:
    enum class Type
    {
        CBV_SRV_UAV,
        RTV,
        DSV,
        Sampler
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

struct DescriptorHandleInfo {
    D3D12_CPU_DESCRIPTOR_HANDLE startCPU;
    D3D12_GPU_DESCRIPTOR_HANDLE startGPU;
    uint                        handleSize;
    ID3D12DescriptorHeap*       ownerHeap = nullptr;
};

class DX12DescriptorSet : public IDescriptorSet
{
public:
    DX12DescriptorSet( ID3D12Device*        device,
                       DescriptorHandleInfo views,
                       DescriptorHandleInfo samplers );
    ~DX12DescriptorSet() override;

    void attach( uint binding, ITexture* tex, ResourceState bindingState ) override;
    void attach( uint binding, IBuffer* buf, ResourceState bindingState ) override;
    void attach( uint binding, ISampler* samp ) override;
    void attach( uint binding, IAccel* accel ) override;
    void attachDynamic( uint binding, IBuffer* buf, ulong offset, ulong range, uint stride, ResourceState bindingState ) override;
    void attachBufferView( uint binding, const BufferView& bufferView, ResourceState bindingState ) override;

    void attachBindless( uint binding, uint arrayIndex, ITexture* tex, ResourceState bindingState ) override;
    void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<ITexture*>& textures, ResourceState bindingState ) override;
    void attachBindless( uint binding, uint arrayIndex, IBuffer* buf, ResourceState bindingState ) override;
    void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<IBuffer*>& buffers, ResourceState bindingState ) override;
    void attachBindless( uint binding, uint arrayIndex, ISampler* samp ) override;
    void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<ISampler*>& samplers ) override;
    void attachBindless( uint binding, uint arrayIndex, IAccel* accel ) override;
    void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<IAccel*>& accels ) override;

    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    D3D12_GPU_DESCRIPTOR_HANDLE getViewGPUHandle() const { return _views.startGPU; }
    ID3D12DescriptorHeap*       getViewOwnerHeap() const { return _views.ownerHeap; }
    D3D12_GPU_DESCRIPTOR_HANDLE getSamplerGPUHandle() const { return _samplers.startGPU; }
    ID3D12DescriptorHeap*       getSamplerOwnerHeap() const { return _samplers.ownerHeap; }

    void reconfigure( D3D12_CPU_DESCRIPTOR_HANDLE cpuViewHandle,
                      D3D12_GPU_DESCRIPTOR_HANDLE gpuViewHandle,
                      D3D12_CPU_DESCRIPTOR_HANDLE cpuSamplerHandle,
                      D3D12_GPU_DESCRIPTOR_HANDLE gpuSamplerHandle ) {
        _views.startCPU    = cpuViewHandle;
        _views.startGPU    = gpuViewHandle;
        _samplers.startCPU = cpuSamplerHandle;
        _samplers.startGPU = gpuSamplerHandle;
    }

private:
    ID3D12Device* _device;
    // Views
    DescriptorHandleInfo _views = {};
    // Samplers
    DescriptorHandleInfo _samplers = {};
};

class DX12DescriptorAllocator : public IDescriptorAllocator
{
public:
    DX12DescriptorAllocator( ID3D12Device* device, const DescriptorAllocatorDesc& desc );
    ~DX12DescriptorAllocator() override;

    IDescriptorSet*                allocate( IPipelineLayout* layout, uint setIndex ) override;
    void                           reset() override;
    const DescriptorAllocatorDesc& getDescription() const override { return _desc; }

    void             setDebugName( std::string_view name ) override;
    std::string_view getDebugName() const override;
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    void lockPersistent() override;
    void unlockPersistent() override;

private:
    ID3D12Device*           _device;
    DescriptorAllocatorDesc _desc;

    DX12DescriptorHeap _viewHeap;
    uint               _currentViewOffset = 0;
    uint               _viewHandleSize    = 0;

    DX12DescriptorHeap _samplerHeap;
    uint               _currentSamplerOffset = 0;
    uint               _samplerHandleSize    = 0;

    // Pooling
    std::vector<Memory::OwnerPtr<DX12DescriptorSet>> _setPool;
    uint                                             _poolIndex = 0;

    // Persistent Views
    uint _persistentViewOffset    = 0;
    uint _persistentSamplerOffset = 0;
    uint _persistentPoolIndex     = 0;
};
} // namespace Graphics::RHI

AXION_NAMESPACE_END