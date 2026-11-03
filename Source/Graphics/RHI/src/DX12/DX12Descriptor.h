#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IDescriptor.h"
#include "DX12Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

#pragma region Heap
class DX12DescriptorHeap;
/// offset = first descriptor index in the heap, size = descriptor count
using DescriptorHeapAllocation = Memory::SubAllocation<DX12DescriptorHeap>;

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

    enum class AllocationPolicy
    {
        Linear,  ///< Bump allocation, only reset() frees. (eg: swapchain RTVs)
        FreeList ///< Individual allocate/free with coalescing. (eg: global shader-visible heap, resource views)
    };

    DX12DescriptorHeap() = default;

    AXION_DISABLE_COPY_AND_MOVE(DX12DescriptorHeap)

    void initialize( ID3D12Device* device, Type type, u32 numDescriptors, AllocationPolicy policy, bool shaderVisible = false );

    DescriptorHeapAllocation    allocate( u32 count = 1 );                    ///< Contiguous block of descriptors.
    void                        free( DescriptorHeapAllocation& allocation ); ///< FreeList only.
    void                        reset();                                      ///< Frees everything.
    D3D12_CPU_DESCRIPTOR_HANDLE allocateCPU();                                ///< Shortcut: allocate( 1 ) + getCPU.

    D3D12_CPU_DESCRIPTOR_HANDLE getCPU( u32 index ) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPU( u32 index ) const; ///< Shader-visible heaps only.
    u32                         getIndex( D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle ) const;

    ID3D12DescriptorHeap* getHeap() const { return _heap.Get(); }
    Type                  getType() const { return _type; }
    AllocationPolicy      getPolicy() const { return _policy; }
    u32                   getDescriptorSize() const { return _descriptorSize; }
    bool                  isShaderVisible() const { return _shaderVisible; }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUStart() const { return _baseCPU; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUStart() const { return _baseGPU; }

    void setDebugName( StringView name );

    // Container contract for Memory::SubAllocators
    u64        getCapacity() const { return _capacity; }
    StringView getDebugName() const { return _debugName; }

private:
    using SubAllocator = Memory::ISubAllocator<DX12DescriptorHeap, Memory::VisibilityNone>;

    ComPtr<ID3D12DescriptorHeap> _heap           = nullptr;
    u32                          _descriptorSize = 0;
    u32                          _capacity       = 0;
    bool                         _shaderVisible  = false;
    D3D12_CPU_DESCRIPTOR_HANDLE  _baseCPU {};
    D3D12_GPU_DESCRIPTOR_HANDLE  _baseGPU {};
    Type                         _type;
    AllocationPolicy             _policy;
    String64                     _debugName;

    Memory::OwnerPtr<SubAllocator> _allocator;
};

struct DescriptorHandleInfo {
    D3D12_CPU_DESCRIPTOR_HANDLE startCPU;
    D3D12_GPU_DESCRIPTOR_HANDLE startGPU;
    u32                         handleSize;
    ID3D12DescriptorHeap*       ownerHeap = nullptr;
};
#pragma region Descriptor Set

class DX12DescriptorSet : public IDescriptorSet
{
public:
    DX12DescriptorSet( ID3D12Device*                 device,
                       const DescriptorHandleInfo&   views,
                       const DescriptorHandleInfo&   samplers,
                       const D3D12BindingMappingLUT& bindingOffsets );
    ~DX12DescriptorSet() override;

    void attach( u32 regBinding, DescriptorType descType, ITexture* tex ) override;
    void attach( u32 regBinding, DescriptorType descType, IBuffer* buf ) override;
    void attach( u32 regBinding, ISampler* samp ) override;
    void attach( u32 regBinding, IAccel* accel ) override;
    void attachDynamic( u32 regBinding, DescriptorType descType, IBuffer* buf, u64 offset, u64 range, u32 stride ) override;
    void attachBufferSlice( u32 regBinding, DescriptorType descType, const BufferSlice& bufferSlice ) override;

    void attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, ITexture* tex ) override;
    void attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<ITexture*>& textures ) override;
    void attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, IBuffer* buf ) override;
    void attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<IBuffer*>& buffers ) override;
    void attachBindless( u32 regBinding, u32 arrayIndex, ISampler* samp ) override;
    void attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<ISampler*>& samplers ) override;
    void attachBindless( u32 regBinding, u32 arrayIndex, IAccel* accel ) override;
    void attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<IAccel*>& accels ) override;

    NativeObject getNativeObject( ObjectType objectType ) override;
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

    D3D12_GPU_DESCRIPTOR_HANDLE getViewGPUHandle() const { return _views.startGPU; }
    ID3D12DescriptorHeap*       getViewOwnerHeap() const { return _views.ownerHeap; }
    D3D12_GPU_DESCRIPTOR_HANDLE getSamplerGPUHandle() const { return _samplers.startGPU; }
    ID3D12DescriptorHeap*       getSamplerOwnerHeap() const { return _samplers.ownerHeap; }

    void reconfigure( const DescriptorHandleInfo&   views,
                      const DescriptorHandleInfo&   samplers,
                      const D3D12BindingMappingLUT& bindingOffsets ) {
        _views           = views;
        _samplers        = samplers;
        _bindingMappings = bindingOffsets;
    }

private:
    u32                         getD3D12BindingOffset( u32 regBinding, D3D12_DESCRIPTOR_RANGE_TYPE rangeType ) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDestHandle( u32 regBinding, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, u32 arrayIndex ) const;

    ID3D12Device* _device;
    // Views
    DescriptorHandleInfo _views = {};
    // Samplers
    DescriptorHandleInfo _samplers = {};
    // Offset Mapping for each binding
    D3D12BindingMappingLUT _bindingMappings = {};
};

#pragma region Descriptor Allocator

class DX12DescriptorAllocator : public IDescriptorAllocator
{
public:
    DX12DescriptorAllocator( ID3D12Device*                  device,
                             DX12DescriptorHeap&            gpuHeapSRV,
                             DX12DescriptorHeap&            gpuHeapSamplers,
                             const DescriptorAllocatorDesc& desc );
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

    // Reserved blocks inside the device global shader-visible heaps. Offsets below are relative to them
    DX12DescriptorHeap&      _gpuHeapSRV;
    DX12DescriptorHeap&      _gpuHeapSamplers;
    DescriptorHeapAllocation _viewRange;
    DescriptorHeapAllocation _samplerRange;

    u32 _currentViewOffset    = 0;
    u32 _currentSamplerOffset = 0;

    // Pooling
    STLW::Vector<Memory::OwnerPtr<DX12DescriptorSet>> _setPool;
    u32                                               _poolIndex = 0;

    // Persistent Views
    u32 _persistentViewOffset    = 0;
    u32 _persistentSamplerOffset = 0;
    u32 _persistentPoolIndex     = 0;
};
} // namespace Graphics::RHI

AXION_NAMESPACE_END