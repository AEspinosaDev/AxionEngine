#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/IResource.h"
#include "DX12Device.h"
#include "StateTracking.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( DX12Texture, DX12Texture )

class DX12Texture : public ITexture
{
public:
    DX12Texture( const TextureDesc&   desc,
                 DX12Device::Context& ctx,
                 const void*          initialData = nullptr );
    DX12Texture( const ComPtr<ID3D12Resource>& resource,
                 const TextureDesc&            desc,
                 DX12Device::Context&          ctx,
                 bool                          useDecriptionParams );
    ~DX12Texture() override;

    const TextureDesc&    getDescription() const override;
    NativeObject          getNativeObject( ObjectType objectType ) override;
    ResourceState         getCurrentState() const override { return _stateTracker.getCurrentState(); }
    ResourceStateTracker& stateTracker();

    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override { return _desc.debugName; };
    STLW::String toString() const override;

    u64 getDeviceAddress() const override { return _resource->GetGPUVirtualAddress(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const { return _srvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getRTV() const { return _rtvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getDSV() const { return _dsvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getUAV() const { return _uavHandle; }

private:
    void createViews( DX12Device::Context& ctx, bool useDescriptionParams );
    void uploadInitialData( DX12Device::Context& ctx, const void* initialData );

    TextureDesc          _desc;
    ResourceStateTracker _stateTracker;

    ComPtr<ID3D12Resource>      _resource;
    ComPtr<D3D12MA::Allocation> _allocation;

    // CPU handles for views
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _uavHandle = {};
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12Buffer, DX12Buffer )

class DX12Buffer : public IBuffer
{
public:
    DX12Buffer(
        const BufferDesc&    desc,
        DX12Device::Context& ctx,
        const void*          initialData = nullptr );
    ~DX12Buffer() override;

    const BufferDesc&     getDescription() const override { return _desc; }
    void                  copyData( const void* data, u64 size, u64 offset = 0 ) override;
    void*                 getData() const override;
    NativeObject          getNativeObject( ObjectType objectType ) override;
    ResourceState         getCurrentState() const override { return _stateTracker.getCurrentState(); }
    ResourceStateTracker& stateTracker() { return _stateTracker; };

    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override { return _desc.debugName; };
    STLW::String toString() const override;

    u64 getDeviceAddress() const override { return _resource->GetGPUVirtualAddress(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const { return _srvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getCBV() const { return _cbvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getUAV() const { return _uavHandle; }
    D3D12_VERTEX_BUFFER_VIEW    getVBV() const;
    D3D12_INDEX_BUFFER_VIEW     getIBV() const;

    void* map() override;
    void  unmap() override;

private:
    void createViews( DX12Device::Context& ctx );
    void uploadInitialData( DX12Device::Context& ctx, const void* initialData );

private:
    BufferDesc           _desc {};
    ResourceStateTracker _stateTracker;

    ComPtr<D3D12MA::Allocation> _allocation;
    ComPtr<ID3D12Resource>      _resource;

    // CPU descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _cbvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _uavHandle = {};
    D3D12_VERTEX_BUFFER_VIEW    _vbv       = {};
    D3D12_INDEX_BUFFER_VIEW     _ibv       = {};

    // Raw mapped data
    void* _mappedPtr = nullptr;
};

class DX12Sampler : public ISampler
{
public:
    DX12Sampler( const SamplerDesc& desc, DX12Device::Context& ctx );
    ~DX12Sampler() override;

    const Description& getDescription() const override { return _desc; };
    NativeObject       getNativeObject( ObjectType objectType ) override;

    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override { return _desc.debugName; };
    STLW::String toString() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE getSamplerHandle() const { return _samplerHandle; }

private:
    SamplerDesc _desc;

    D3D12_CPU_DESCRIPTOR_HANDLE _samplerHandle = {};
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12Accel, DX12Accel )

class DX12Accel : public IAccel
{
public:
    DX12Accel( const AccelDesc& desc, DX12Device::Context& ctx, bool immediateBuild );
    ~DX12Accel() override;

    const Description& getDescription() const override { return _desc; };
    NativeObject       getNativeObject( ObjectType objectType ) override;

    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override { return _desc.debugName; };
    STLW::String toString() const override;

    AccelType getType() const override;
    u64     getDeviceAddress() const override;

    u64 getUpdateScratchSize() const override;
    u64 getBuildScratchSize() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const { return _srvHandle; }

    static void prepareInputs( const AccelDesc&                                      desc,
                               D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& outInputs,
                               STLW::Vector<D3D12_RAYTRACING_GEOMETRY_DESC>&         outGeoms );

private:
    void      createView( DX12Device::Context& ctx );
    AccelDesc _desc;

    Memory::OwnerPtr<DX12Buffer> _buffer = nullptr;

    u64 _deviceAddress = 0;

    u64 _updateScratchSize = 0;
    u64 _buildScratchSize  = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle = {};

    bool _isBuilt = false;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END