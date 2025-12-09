#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/Resource.h"
#include "DX12Device.hpp"
#include "StateTracking.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( DX12Texture, DX12Texture )

class DX12Texture : public RefCounter<ITexture>
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
    void                  setDebugName( const std::string& name ) override;
    const std::string&    getDebugName() const override;
    NativeObject          getNativeObject( ObjectType objectType ) override;
    ResourceState         getCurrentState() const override { return _stateTracker.getCurrentState(); }
    ResourceStateTracker& stateTracker();
    std::string           toString() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const { return _srvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getRTV() const { return _rtvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getDSV() const { return _dsvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE getUAV() const { return _uavHandle; }

private:
    void createViews( DX12Device::Context& ctx, bool useDescriptionParams );
    void uploadInitialData( DX12Device::Context& ctx, const void* initialData );

    TextureDesc          _desc;
    ResourceStateTracker _stateTracker;

    ComPtr<ID3D12Resource> _resource;

    // CPU handles for views
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _uavHandle = {};
};

DEFINE_COM_PTR_FOR_TYPE( DX12Buffer, DX12Buffer )

class DX12Buffer : public RefCounter<IBuffer>
{
public:
    DX12Buffer(
        const BufferDesc&    desc,
        DX12Device::Context& ctx,
        const void*          initialData = nullptr );
    ~DX12Buffer() override;

    const BufferDesc&     getDescription() const override { return _desc; }
    void                  copyData( const void* data, ulong size, ulong offset = 0 ) override;
    void                  setDebugName( const std::string& name ) override;
    const std::string&    getDebugName() const override { return _desc.debugName; }
    NativeObject          getNativeObject( ObjectType objectType ) override;
    std::string           toString() const override;
    ResourceState         getCurrentState() const override { return _stateTracker.getCurrentState(); }
    ResourceStateTracker& stateTracker() { return _stateTracker; };

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

    ComPtr<ID3D12Resource> _resource;

    // CPU descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _cbvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE _uavHandle = {};
    D3D12_VERTEX_BUFFER_VIEW    _vbv       = {};
    D3D12_INDEX_BUFFER_VIEW     _ibv       = {};
};


class DX12Sampler : public RefCounter<ISampler>
{
public:
    DX12Sampler( const SamplerDesc& desc, DX12Device::Context& ctx );
    ~DX12Sampler() override;

    const Description& getDescription() const override { return _desc; };
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE getSamplerHandle() const { return _samplerHandle; }

private:
    SamplerDesc _desc;

    D3D12_CPU_DESCRIPTOR_HANDLE _samplerHandle = {};
};


DEFINE_COM_PTR_FOR_TYPE( DX12Accel, DX12Accel )

class DX12Accel : public RefCounter<IAccel>
{
public:
    DX12Accel( const AccelDesc& desc, DX12Device::Context& ctx );
    ~DX12Accel() override;

    const Description& getDescription() const override { return _desc; };
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    AccelType getType() const override;
    ulong     getDeviceAddress() const override;

private:
    AccelDesc _desc;

    std::unique_ptr<DX12Buffer> _buffer      = nullptr;
    ulong                       _deviceAddress = 0;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END