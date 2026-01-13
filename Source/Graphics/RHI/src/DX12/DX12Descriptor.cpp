#pragma once
#include "DX12Descriptor.h"
#include "DX12Debug.hpp"
#include "DX12Resource.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {
void DX12DescriptorHeap::init( ID3D12Device* device, Type type, uint numDescriptors, bool shaderVisible ) {

    _capacity = numDescriptors;
    _type     = type;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors             = numDescriptors;

    switch ( type )
    {
        case Type::CBV_SRV_UAV:
            desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            break;
        case Type::RTV:
            desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            break;
        case Type::DSV:
            desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            break;
        case Type::Sampler:
            desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            break;
    }

    DX_CHECK( device->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &_heap ) ) );
    _descriptorSize = device->GetDescriptorHandleIncrementSize( desc.Type );
    _baseCPU        = _heap->GetCPUDescriptorHandleForHeapStart();
    if ( shaderVisible )
        _baseGPU = _heap->GetGPUDescriptorHandleForHeapStart();
}

void DX12DescriptorHeap::reset() {
    _allocated = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::allocateCPU() {
    AXION_LOG_ASSERT( _allocated < _capacity, Logger::Module::RHI, "FATAL | Out of descriptor heap space" );

    D3D12_CPU_DESCRIPTOR_HANDLE handle = {
        _baseCPU.ptr + _allocated * _descriptorSize };
    ++_allocated;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::getGPU( D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle ) const {
    AXION_LOG_ASSERT( _type == Type::CBV_SRV_UAV, Logger::Module::RHI, "FATAL | This can only be used with CBV_SRV_UAV Type Heaps" );
    uint                        offset = static_cast<uint>( ( cpuHandle.ptr - _baseCPU.ptr ) / _descriptorSize );
    D3D12_GPU_DESCRIPTOR_HANDLE gpu    = { _baseGPU.ptr + offset * _descriptorSize };
    return gpu;
}

void DX12DescriptorHeap::setDebugName( const std::string& name ) {
    _heap->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

DX12DescriptorSet::DX12DescriptorSet( ID3D12Device* device, DescriptorHandleInfo views, DescriptorHandleInfo samplers )
    : _device( device )
    , _views( views )
    , _samplers( samplers ) {
}

DX12DescriptorSet::~DX12DescriptorSet() {
}

void DX12DescriptorSet::attach( uint binding, ITexture* tex, ResourceState bindingState ) {
    AXION_LOG_ASSERT( tex, Logger::Module::RHI, "Binding null texture!" );
    auto* dxTex = static_cast<DX12Texture*>( tex );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _views.startCPU;
    dest.ptr += binding * _views.handleSize;

    D3D12_CPU_DESCRIPTOR_HANDLE src;

    if ( bindingState == ResourceState::UnorderedAccess )
        src = dxTex->getUAV();
    else
        src = dxTex->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attach( uint binding, IBuffer* buf, ResourceState bindingState ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );
    auto* dxBuf = static_cast<DX12Buffer*>( buf );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _views.startCPU;
    dest.ptr += binding * _views.handleSize;

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    switch ( bindingState )
    {
        case ResourceState::UnorderedAccess:
            src = dxBuf->getUAV();
            break;
        case ResourceState::ShaderResource:
            src = dxBuf->getSRV();
            break;
        case ResourceState::ConstantBuffer:
            src = dxBuf->getCBV();
            break;

        default:
            break;
    }

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachDynamic( uint binding, IBuffer* buf, ulong offset, ulong range, uint stride, ResourceState bindingState ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );

    auto* dxBuf     = static_cast<DX12Buffer*>( buf );
    ID3D12Resource* d3dRes = dxBuf->getNativeObject( ObjectTypes::DX12_Resource );

    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = _views.startCPU;
    destHandle.ptr += binding * _views.handleSize;

    if ( range == 0 )
        range = buf->getDescription().size - offset;

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = d3dRes->GetGPUVirtualAddress() + offset;

    // --- STRIDE LOGIC ---
    uint finalStride = stride;
    if ( finalStride == 0 )
        finalStride = 4;

    switch ( bindingState )
    {
        case ResourceState::ConstantBuffer: {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = gpuAddress;
            cbvDesc.SizeInBytes = (UINT)Helpers::alignUp( range, (size_t)256 );

            _device->CreateConstantBufferView( &cbvDesc, destHandle );
            break;
        }

        case ResourceState::ShaderResource: {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            bool isRaw = dxBuf->getDescription().allowRawViews && ( stride == 0 );

            if ( isRaw )
            {
                srvDesc.Format                     = DXGI_FORMAT_R32_TYPELESS;
                srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_RAW;
                srvDesc.Buffer.FirstElement        = offset / 4;
                srvDesc.Buffer.NumElements         = range / 4;
                srvDesc.Buffer.StructureByteStride = 0;
            } else
            {
                // Structured Buffer
                srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
                srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
                srvDesc.Buffer.StructureByteStride = finalStride;

                // IMPORTANT: In DX12, FirstElement is index-based, not byte-based for Structured.
                // Constraint: offset must be a multiple of stride.
                srvDesc.Buffer.FirstElement = offset / finalStride;
                srvDesc.Buffer.NumElements  = range / finalStride;
            }

            _device->CreateShaderResourceView( d3dRes, &srvDesc, destHandle );
            break;
        }

        case ResourceState::UnorderedAccess: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;

            // Similar logic for RAW vs Structured UAV
            bool isRaw = dxBuf->getDescription().allowRawViews && ( stride == 0 );

            if ( isRaw )
            {
                uavDesc.Format                     = DXGI_FORMAT_R32_TYPELESS;
                uavDesc.Buffer.Flags               = D3D12_BUFFER_UAV_FLAG_RAW;
                uavDesc.Buffer.FirstElement        = offset / 4;
                uavDesc.Buffer.NumElements         = range / 4;
                uavDesc.Buffer.StructureByteStride = 0;
            } else
            {
                uavDesc.Format                     = DXGI_FORMAT_UNKNOWN;
                uavDesc.Buffer.Flags               = D3D12_BUFFER_UAV_FLAG_NONE;
                uavDesc.Buffer.StructureByteStride = finalStride;
                uavDesc.Buffer.FirstElement        = offset / finalStride;
                uavDesc.Buffer.NumElements         = range / finalStride;
            }

            _device->CreateUnorderedAccessView( d3dRes, nullptr, &uavDesc, destHandle );
            break;
        }

        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "Unsupported or invalid binding state for buffer attachment" );
            break;
    }
}

void DX12DescriptorSet::attach( uint binding, ISampler* samp ) {
    AXION_LOG_ASSERT( samp, Logger::Module::RHI, "Binding null sampler!" );
    auto* dxSamp = static_cast<DX12Sampler*>( samp );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _samplers.startCPU;
    dest.ptr += binding * _samplers.handleSize;
    D3D12_CPU_DESCRIPTOR_HANDLE src = dxSamp->getSamplerHandle();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
}

void DX12DescriptorSet::attach( uint binding, IAccel* accel ) {
    AXION_LOG_ASSERT( accel, Logger::Module::RHI, "Binding null Accel!" );
    auto* dxAccel = static_cast<DX12Accel*>( accel );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _views.startCPU;
    dest.ptr += binding * _views.handleSize;

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    src = dxAccel->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::setDebugName( const std::string& name ) {
}

const std::string& DX12DescriptorSet::getDebugName() const {
    return std::string();
}

NativeObject DX12DescriptorSet::getNativeObject( ObjectType objectType ) {
    AXION_LOG_WARN( Logger::Module::RHI, "DX12 Descriptor Set | No Native Object" );
    return nullptr;
}

std::string DX12DescriptorSet::toString() const {
    return std::string();
}

DX12DescriptorAllocator::DX12DescriptorAllocator( ID3D12Device*                  device,
                                                  const DescriptorAllocatorDesc& desc )

    : _device( device )
    , _desc( desc ) {

    _viewHeap.init( device, DX12DescriptorHeap::Type::CBV_SRV_UAV, _desc.numViews, true );
    _viewHandleSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    _samplerHeap.init( device, DX12DescriptorHeap::Type::Sampler, _desc.numSamplers, true );
    _samplerHandleSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

    _setPool.reserve( _desc.numDescriptors );
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Desc.Allocator created [{}]", _desc.debugName );
}
DX12DescriptorAllocator::~DX12DescriptorAllocator() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Desc.Allocator [{}]", _desc.debugName );
}

IDescriptorSet* DX12DescriptorAllocator::allocate( IPipelineLayout* layout, uint setIndex ) {

    uint viewCount    = layout->getViewCount( setIndex );
    uint samplerCount = layout->getSamplerCount( setIndex );

    if ( _currentViewOffset + viewCount > _desc.numViews )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "View Descriptor Heap Overflow! Increase Views size." );
        return nullptr;
    }
    if ( _currentSamplerOffset + samplerCount > _desc.numSamplers )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Sampler Descriptor Heap Overflow! Increase Samplers size." );
        return nullptr;
    }

    // Views
    DescriptorHandleInfo viewInfo = {};
    if ( viewCount > 0 )
    {
        viewInfo.startCPU   = _viewHeap.getCPUStart();
        viewInfo.startGPU   = _viewHeap.getGPUStart();
        viewInfo.handleSize = _viewHandleSize;
        viewInfo.ownerHeap  = _viewHeap.getHeap();

        viewInfo.startCPU.ptr += _currentViewOffset * _viewHandleSize;
        viewInfo.startGPU.ptr += _currentViewOffset * _viewHandleSize;

        _currentViewOffset += viewCount;
    }
    // Samplers
    DescriptorHandleInfo samplerInfo = {};
    if ( samplerCount > 0 )
    {
        samplerInfo.startCPU   = _samplerHeap.getCPUStart();
        samplerInfo.startGPU   = _samplerHeap.getGPUStart();
        samplerInfo.handleSize = _samplerHandleSize;
        samplerInfo.ownerHeap  = _samplerHeap.getHeap();

        samplerInfo.startCPU.ptr += _currentSamplerOffset * _samplerHandleSize;
        samplerInfo.startGPU.ptr += _currentSamplerOffset * _samplerHandleSize;

        _currentSamplerOffset += samplerCount;
    }

    if ( _poolIndex < _setPool.size() )
    {
        // Recycle
        _setPool[_poolIndex]->reconfigure(
            viewInfo.startCPU, viewInfo.startGPU, samplerInfo.startCPU, samplerInfo.startGPU );
        return _setPool[_poolIndex++].get();
    } else
    {
        // New
        auto newSet = std::make_unique<DX12DescriptorSet>(
            _device, viewInfo, samplerInfo );
        IDescriptorSet* ret = newSet.get();
        _setPool.push_back( std::move( newSet ) );
        _poolIndex++;
        return ret;
    }
}

void DX12DescriptorAllocator::reset() {
    _viewHeap.reset();
    _currentViewOffset = 0;

    _samplerHeap.reset();
    _currentSamplerOffset = 0;

    _poolIndex = 0;
}

void DX12DescriptorAllocator::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _viewHeap.setDebugName( _desc.debugName + "| Views Heap" );
    _samplerHeap.setDebugName( _desc.debugName + "| Samplers Heap" );
}

const std::string& DX12DescriptorAllocator::getDebugName() const {
    return _desc.debugName;
}

NativeObject DX12DescriptorAllocator::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_DescriptorHeap:
            return NativeObject( objectType, _viewHeap.getHeap() );
        case ObjectTypes::DX12_DescriptorSamplerHeap:
            return NativeObject( objectType, _samplerHeap.getHeap() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Descriptor Allocator | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12DescriptorAllocator::toString() const {
    return std::string();
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END