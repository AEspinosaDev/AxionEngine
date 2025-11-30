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

DX12DescriptorSet::DX12DescriptorSet( ID3D12Device*               device,
                                      ID3D12DescriptorHeap*       ownerHeap,
                                      D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                                      D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
                                      uint                        descriptorSize )
    : _device( device )
    , _startCPU( cpuHandle )
    , _startGPU( gpuHandle )
    , _handleSize( descriptorSize )
    , _ownerHeap( ownerHeap ) {
}

DX12DescriptorSet::~DX12DescriptorSet() {
}

void DX12DescriptorSet::bind( uint binding, ITexture* tex, ResourceState usage ) {
    AXION_LOG_ASSERT( tex, Logger::Module::RHI, "Binding null texture!" );
    auto* dxTex = static_cast<DX12Texture*>( tex );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _startCPU;
    dest.ptr += binding * _handleSize;

    D3D12_CPU_DESCRIPTOR_HANDLE src;

    if ( usage == ResourceState::UnorderedAccess )
        src = dxTex->getUAV();
    else
        src = dxTex->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::bind( uint binding, IBuffer* buf, ResourceState usage ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );
    auto* dxBuf = static_cast<DX12Buffer*>( buf );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = _startCPU;
    dest.ptr += binding * _handleSize;

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    if ( usage == ResourceState::UnorderedAccess )
        src = dxBuf->getUAV();
    else
        src = dxBuf->getSRV();

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

    _heap.init( device, DX12DescriptorHeap::Type::CBV_SRV_UAV, _desc.numDescriptors, true );
    _handleSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    _setPool.reserve( _desc.numDescriptors );
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Desc.Allocator created [{}]", _desc.debugName );
}
DX12DescriptorAllocator::~DX12DescriptorAllocator() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Desc.Allocator [{}]", _desc.debugName );
}

IDescriptorSet* DX12DescriptorAllocator::allocate( IPipelineLayout* layout, uint setIndex ) {

    uint count = (uint)layout->getDescription().sets[setIndex].bindings.size();

    if ( _currentOffset + count > _desc.numDescriptors )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Descriptor Heap Overflow! Increase size." );
        return nullptr;
    }

    auto cpuBase = _heap.getCPUStart();
    auto gpuBase = _heap.getGPUStart();

    cpuBase.ptr += _currentOffset * _handleSize;
    gpuBase.ptr += _currentOffset * _handleSize;

    _currentOffset += count;

    if ( _poolIndex < _setPool.size() )
    {
        // CASO A: Reciclar existente
        _setPool[_poolIndex]->reconfigure( cpuBase, gpuBase );

        return _setPool[_poolIndex++].get();
    } else
    {
        // CASO B: Crear Nuevo
        ID3D12DescriptorHeap* nativeHeap = _heap.getHeap();
        auto                  newSet     = std::make_unique<DX12DescriptorSet>( _device, nativeHeap, cpuBase, gpuBase, _handleSize );
        IDescriptorSet*       ret        = newSet.get();

        _setPool.push_back( std::move( newSet ) );
        _poolIndex++;

        return ret;
    }
}

void DX12DescriptorAllocator::reset() {
    _heap.reset();
    _currentOffset = 0;
    _poolIndex     = 0;
}

void DX12DescriptorAllocator::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _heap.setDebugName( _desc.debugName );
}

const std::string& DX12DescriptorAllocator::getDebugName() const {
    return _desc.debugName;
}

NativeObject DX12DescriptorAllocator::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_DescriptorHeap:
            return NativeObject( objectType, _heap.getHeap() );
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