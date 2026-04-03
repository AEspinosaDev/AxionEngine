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

    void init( ID3D12Device* device, Type type, u32 numDescriptors, bool shaderVisible = false );
    void reset();

    D3D12_CPU_DESCRIPTOR_HANDLE allocateCPU();
    D3D12_GPU_DESCRIPTOR_HANDLE getGPU( D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle ) const;

    ID3D12DescriptorHeap* getHeap() const { return _heap.Get(); }
    Type                  getType() const { return _type; }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUStart() const { return _baseCPU; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUStart() const { return _baseGPU; }

    void setDebugName( StringView name );

private:
    ComPtr<ID3D12DescriptorHeap> _heap           = nullptr;
    u32                         _descriptorSize = 0;
    u32                         _allocated      = 0;
    u32                         _capacity       = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE  _baseCPU {};
    D3D12_GPU_DESCRIPTOR_HANDLE  _baseGPU {};
    Type                         _type;
};

struct DescriptorHandleInfo {
    D3D12_CPU_DESCRIPTOR_HANDLE startCPU;
    D3D12_GPU_DESCRIPTOR_HANDLE startGPU;
    u32                        handleSize;
    ID3D12DescriptorHeap*       ownerHeap = nullptr;
};

class DX12DescriptorSet : public IDescriptorSet
{
public:
    DX12DescriptorSet( ID3D12Device*        device,
                       DescriptorHandleInfo views,
                       DescriptorHandleInfo samplers );
    ~DX12DescriptorSet() override;

    void attach( u32 binding, ITexture* tex, ResourceState bindingState ) override;
    void attach( u32 binding, IBuffer* buf, ResourceState bindingState ) override;
    void attach( u32 binding, ISampler* samp ) override;
    void attach( u32 binding, IAccel* accel ) override;
    void attachDynamic( u32 binding, IBuffer* buf, u64 offset, u64 range, u32 stride, ResourceState bindingState ) override;
    void attachBufferView( u32 binding, const BufferView& bufferView, ResourceState bindingState ) override;

    void attachBindless( u32 binding, u32 arrayIndex, ITexture* tex, ResourceState bindingState ) override;
    void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<ITexture*>& textures, ResourceState bindingState ) override;
    void attachBindless( u32 binding, u32 arrayIndex, IBuffer* buf, ResourceState bindingState ) override;
    void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<IBuffer*>& buffers, ResourceState bindingState ) override;
    void attachBindless( u32 binding, u32 arrayIndex, ISampler* samp ) override;
    void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<ISampler*>& samplers ) override;
    void attachBindless( u32 binding, u32 arrayIndex, IAccel* accel ) override;
    void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<IAccel*>& accels ) override;

    NativeObject getNativeObject( ObjectType objectType ) override;
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

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

    IDescriptorSet*                allocate( IPipelineLayout* layout, u32 setIndex ) override;
    void                           reset() override;
    const DescriptorAllocatorDesc& getDescription() const override { return _desc; }

    NativeObject getNativeObject( ObjectType objectType ) override;
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

    void lockPersistent() override;
    void unlockPersistent() override;

private:
    ID3D12Device*           _device;
    DescriptorAllocatorDesc _desc;

    DX12DescriptorHeap _viewHeap;
    u32               _currentViewOffset = 0;
    u32               _viewHandleSize    = 0;

    DX12DescriptorHeap _samplerHeap;
    u32               _currentSamplerOffset = 0;
    u32               _samplerHandleSize    = 0;

    // Pooling
    STLW::Vector<Memory::OwnerPtr<DX12DescriptorSet>> _setPool;
    u32                                              _poolIndex = 0;

    // Persistent Views
    u32 _persistentViewOffset    = 0;
    u32 _persistentSamplerOffset = 0;
    u32 _persistentPoolIndex     = 0;
};
} // namespace Graphics::RHI

AXION_NAMESPACE_END