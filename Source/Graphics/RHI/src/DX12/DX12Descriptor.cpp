#pragma once
#include "DX12Descriptor.h"
#include "DX12Debug.h"
#include "DX12Pipeline.h"
#include "DX12Resource.h"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {
void DX12DescriptorHeap::initialize( ID3D12Device* device, Type type, u32 numDescriptors, bool shaderVisible ) {

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
    u32                         offset = static_cast<u32>( ( cpuHandle.ptr - _baseCPU.ptr ) / _descriptorSize );
    D3D12_GPU_DESCRIPTOR_HANDLE gpu    = { _baseGPU.ptr + offset * _descriptorSize };
    return gpu;
}

void DX12DescriptorHeap::setDebugName( StringView name ) {
    setNativeName( _heap.Get(), name );
}

DX12DescriptorSet::DX12DescriptorSet( ID3D12Device*                 device,
                                      const DescriptorHandleInfo&   views,
                                      const DescriptorHandleInfo&   samplers,
                                      const D3D12BindingMappingLUT& bindingOffsets )
    : _device( device )
    , _views( views )
    , _samplers( samplers )
    , _bindingMappings( bindingOffsets ) {
}

DX12DescriptorSet::~DX12DescriptorSet() {
}

void DX12DescriptorSet::attach( u32 regBinding, DescriptorType descType, ITexture* tex ) {
    AXION_LOG_ASSERT( tex, Logger::Module::RHI, "Binding null texture!" );
    AXION_LOG_ASSERT( descType == DescriptorType::SRV_Image ||
                          descType == DescriptorType::UAV_Image,
                      Logger::Module::RHI,
                      "Invalid descriptor type for texture binding!" );

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE dest      = getDestHandle( regBinding, rangeType, 0 );

    auto*                       dxTex = static_cast<DX12Texture*>( tex );
    D3D12_CPU_DESCRIPTOR_HANDLE src   = ( rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV ) ? dxTex->getUAV() : dxTex->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attach( u32 regBinding, DescriptorType descType, IBuffer* buf ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );
    AXION_LOG_ASSERT( descType == DescriptorType::CBV ||
                          descType == DescriptorType::UAV_Buffer ||
                          descType == DescriptorType::SRV_Buffer,
                      Logger::Module::RHI,
                      "Invalid descriptor type for buffer binding!" );

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE dest      = getDestHandle( regBinding, rangeType, 0 );

    D3D12_CPU_DESCRIPTOR_HANDLE src {};
    auto*                       dxBuf = static_cast<DX12Buffer*>( buf );
    switch ( rangeType )
    {
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            src = dxBuf->getUAV();
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            src = dxBuf->getSRV();
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            src = dxBuf->getCBV();
            break;

        default:
            break;
    }

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attach( u32 regBinding, ISampler* samp ) {
    AXION_LOG_ASSERT( samp, Logger::Module::RHI, "Binding null sampler!" );
    auto* dxSamp = static_cast<DX12Sampler*>( samp );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0 );
    D3D12_CPU_DESCRIPTOR_HANDLE src  = dxSamp->getSamplerHandle();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
}

void DX12DescriptorSet::attach( u32 regBinding, IAccel* accel ) {
    AXION_LOG_ASSERT( accel, Logger::Module::RHI, "Binding null Accel!" );
    auto* dxAccel = static_cast<DX12Accel*>( accel );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 );
    D3D12_CPU_DESCRIPTOR_HANDLE src;
    src = dxAccel->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachDynamic( u32 regBinding, DescriptorType descType, IBuffer* buf, u64 offset, u64 range, u32 stride ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );

    auto*           dxBuf  = static_cast<DX12Buffer*>( buf );
    ID3D12Resource* d3dRes = dxBuf->getNativeObject( ObjectTypes::DX12_Resource );

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType  = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = getDestHandle( regBinding, rangeType, 0 );

    if ( range == 0 )
        range = buf->getDescription().size - offset;

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = d3dRes->GetGPUVirtualAddress() + offset;

    // --- STRIDE LOGIC ---
    u32 finalStride = stride;
    if ( finalStride == 0 )
        finalStride = 4;

    switch ( rangeType )
    {
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = gpuAddress;
            cbvDesc.SizeInBytes                     = (UINT)Helpers::alignUp( range, (size_t)256 );

            _device->CreateConstantBufferView( &cbvDesc, destHandle );
            break;
        }

        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            bool isRaw = dxBuf->getDescription().allowRawViews && ( stride == 0 );

            if ( isRaw )
            {
                srvDesc.Format                     = DXGI_FORMAT_R32_TYPELESS;
                srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_RAW;
                srvDesc.Buffer.FirstElement        = offset / 4;
                srvDesc.Buffer.NumElements         = static_cast<UINT>( range ) / 4;
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
                srvDesc.Buffer.NumElements  = static_cast<UINT>( range ) / finalStride;
            }

            _device->CreateShaderResourceView( d3dRes, &srvDesc, destHandle );
            break;
        }

        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;

            // Similar logic for RAW vs Structured UAV
            bool isRaw = dxBuf->getDescription().allowRawViews && ( stride == 0 );

            if ( isRaw )
            {
                uavDesc.Format                     = DXGI_FORMAT_R32_TYPELESS;
                uavDesc.Buffer.Flags               = D3D12_BUFFER_UAV_FLAG_RAW;
                uavDesc.Buffer.FirstElement        = offset / 4;
                uavDesc.Buffer.NumElements         = static_cast<UINT>( range ) / 4;
                uavDesc.Buffer.StructureByteStride = 0;
            } else
            {
                uavDesc.Format                     = DXGI_FORMAT_UNKNOWN;
                uavDesc.Buffer.Flags               = D3D12_BUFFER_UAV_FLAG_NONE;
                uavDesc.Buffer.StructureByteStride = finalStride;
                uavDesc.Buffer.FirstElement        = offset / finalStride;
                uavDesc.Buffer.NumElements         = static_cast<UINT>( range ) / finalStride;
            }

            _device->CreateUnorderedAccessView( d3dRes, nullptr, &uavDesc, destHandle );
            break;
        }

        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "Unsupported or invalid binding state for buffer attachment" );
            break;
    }
}

void DX12DescriptorSet::attachBufferSlice( u32 regBinding, DescriptorType descType, const BufferSlice& bufferSlice ) {
    attachDynamic( regBinding, descType, bufferSlice.container, bufferSlice.offset, bufferSlice.size, bufferSlice.stride );
}

void DX12DescriptorSet::attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, ITexture* tex ) {
    AXION_LOG_ASSERT( tex, Logger::Module::RHI, "Binding null texture!" );
    auto* dxTex = static_cast<DX12Texture*>( tex );

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType  = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = getDestHandle( regBinding, rangeType, arrayIndex );

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    if ( rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV )
        src = dxTex->getUAV();
    else
        src = dxTex->getSRV();

    _device->CopyDescriptorsSimple( 1, destHandle, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<ITexture*>& textures ) {
    if ( textures.empty() )
        return;

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType  = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = getDestHandle( regBinding, rangeType, startArrayIndex );

    STLW::Vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles( textures.size() );
    STLW::Vector<UINT>                        srcSizes( textures.size(), 1 );
    UINT                                      destSize = (UINT)textures.size();

    for ( size_t i = 0; i < textures.size(); ++i )
    {
        AXION_LOG_ASSERT( textures[i], Logger::Module::RHI, "Binding null texture in array!" );
        auto* dxTex = static_cast<DX12Texture*>( textures[i] );

        if ( rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV )
            srcHandles[i] = dxTex->getUAV();
        else
            srcHandles[i] = dxTex->getSRV();
    }

    _device->CopyDescriptors(
        1, &destHandle, &destSize, (UINT)textures.size(), srcHandles.data(), srcSizes.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, IBuffer* buf ) {
    AXION_LOG_ASSERT( buf, Logger::Module::RHI, "Binding null buffer!" );
    auto* dxBuf = static_cast<DX12Buffer*>( buf );

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType  = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = getDestHandle( regBinding, rangeType, arrayIndex );

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    switch ( rangeType )
    {
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            src = dxBuf->getUAV();
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            src = dxBuf->getSRV();
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            src = dxBuf->getCBV();
            break;

        default:
            break;
    }

    _device->CopyDescriptorsSimple( 1, destHandle, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<IBuffer*>& buffers ) {
    if ( buffers.empty() )
        return;

    D3D12_DESCRIPTOR_RANGE_TYPE rangeType  = DX12Translator::get( descType );
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = getDestHandle( regBinding, rangeType, startArrayIndex );

    STLW::Vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles( buffers.size() );
    STLW::Vector<UINT>                        srcSizes( buffers.size(), 1 );
    UINT                                      destSize = (UINT)buffers.size();

    for ( size_t i = 0; i < buffers.size(); ++i )
    {
        AXION_LOG_ASSERT( buffers[i], Logger::Module::RHI, "Binding null buffer in array!" );
        auto* dxBuf = static_cast<DX12Buffer*>( buffers[i] );

        switch ( rangeType )
        {
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                srcHandles[i] = dxBuf->getUAV();
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                srcHandles[i] = dxBuf->getSRV();
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                srcHandles[i] = dxBuf->getCBV();
                break;
            default:
                break;
        }
    }

    _device->CopyDescriptors(
        1, &destHandle, &destSize, (UINT)buffers.size(), srcHandles.data(), srcSizes.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachBindless( u32 regBinding, u32 arrayIndex, ISampler* samp ) {
    AXION_LOG_ASSERT( samp, Logger::Module::RHI, "Binding null sampler!" );
    auto* dxSamp = static_cast<DX12Sampler*>( samp );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, arrayIndex );
    D3D12_CPU_DESCRIPTOR_HANDLE src  = dxSamp->getSamplerHandle();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
}

void DX12DescriptorSet::attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<ISampler*>& samplers ) {
    if ( samplers.empty() )
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE               dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, startArrayIndex );
    STLW::Vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles( samplers.size() );
    STLW::Vector<UINT>                        srcSizes( samplers.size(), 1 );
    UINT                                      destSize = (UINT)samplers.size();

    for ( size_t i = 0; i < samplers.size(); ++i )
    {
        AXION_LOG_ASSERT( samplers[i], Logger::Module::RHI, "Binding null sampler in array!" );
        auto* dxSamp  = static_cast<DX12Sampler*>( samplers[i] );
        srcHandles[i] = dxSamp->getSamplerHandle();
    }

    _device->CopyDescriptors(
        1, &dest, &destSize, (UINT)samplers.size(), srcHandles.data(), srcSizes.data(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
}

void DX12DescriptorSet::attachBindless( u32 regBinding, u32 arrayIndex, IAccel* accel ) {
    AXION_LOG_ASSERT( accel, Logger::Module::RHI, "Binding null Accel!" );
    auto* dxAccel = static_cast<DX12Accel*>( accel );

    D3D12_CPU_DESCRIPTOR_HANDLE dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, arrayIndex );

    D3D12_CPU_DESCRIPTOR_HANDLE src = dxAccel->getSRV();

    _device->CopyDescriptorsSimple( 1, dest, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<IAccel*>& accels ) {
    if ( accels.empty() )
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE dest = getDestHandle( regBinding, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, startArrayIndex );

    STLW::Vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles( accels.size() );
    STLW::Vector<UINT>                        srcSizes( accels.size(), 1 );
    UINT                                      destSize = (UINT)accels.size();

    for ( size_t i = 0; i < accels.size(); ++i )
    {
        AXION_LOG_ASSERT( accels[i], Logger::Module::RHI, "Binding null accel in array!" );
        auto* dxAccel = static_cast<DX12Accel*>( accels[i] );
        srcHandles[i] = dxAccel->getSRV();
    }

    _device->CopyDescriptors(
        1, &dest, &destSize, (UINT)accels.size(), srcHandles.data(), srcSizes.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

void DX12DescriptorSet::setDebugName( StringView name ) {
    AXION_UNUSED_PARAMETER( name );
}

StringView DX12DescriptorSet::getDebugName() const {
    return "";
}

NativeObject DX12DescriptorSet::getNativeObject( ObjectType /*objectType*/ ) {
    AXION_LOG_WARN( Logger::Module::RHI, "DX12 Descriptor Set | No Native Object" );
    return nullptr;
}

STLW::String DX12DescriptorSet::toString() const {
    return STLW::String();
}

u32 DX12DescriptorSet::getD3D12BindingOffset( u32 regBinding, D3D12_DESCRIPTOR_RANGE_TYPE rangeType ) const {
    const auto& mappings = _bindingMappings.registerMappings[static_cast<u32>( rangeType )];

    for ( const auto& mapping : mappings )
    {
        if ( regBinding >= mapping.hlslBase && regBinding < mapping.hlslBase + mapping.count )
            return mapping.globalOffset + ( regBinding - mapping.hlslBase );
    }

    AXION_LOG_WARN_ONCE( Logger::Module::RHI, "Failed to find binding offset for register {}!", regBinding );
    return 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::getDestHandle( u32 regBinding, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, u32 arrayIndex ) const {
    bool        isSampler = ( rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER );
    const auto& heapInfo  = isSampler ? _samplers : _views;

    D3D12_CPU_DESCRIPTOR_HANDLE dest   = heapInfo.startCPU;
    u32                         offset = getD3D12BindingOffset( regBinding, rangeType );

    // Add base offset and array index
    dest.ptr += ( offset + arrayIndex ) * heapInfo.handleSize;
    return dest;
}

DX12DescriptorAllocator::DX12DescriptorAllocator( ID3D12Device*                  device,
                                                  const DescriptorAllocatorDesc& desc )

    : _device( device )
    , _desc( desc ) {

    _viewHeap.initialize( device, DX12DescriptorHeap::Type::CBV_SRV_UAV, _desc.numViews, true );
    _viewHandleSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    _samplerHeap.initialize( device, DX12DescriptorHeap::Type::Sampler, _desc.numSamplers, true );
    _samplerHandleSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

    _setPool.reserve( _desc.numDescriptors );
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Desc.Allocator created [{}]", _desc.debugName );
}
DX12DescriptorAllocator::~DX12DescriptorAllocator() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Desc.Allocator [{}]", _desc.debugName );
}

IDescriptorSet* DX12DescriptorAllocator::allocate( IPipelineLayout* layout, u32 setIndex ) {

    auto dx12Layout   = static_cast<DX12PipelineLayout*>( layout );
    u32  viewCount    = layout->getViewCount( setIndex );
    u32  samplerCount = layout->getSamplerCount( setIndex );

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
            viewInfo.startCPU, viewInfo.startGPU, samplerInfo.startCPU, samplerInfo.startGPU, dx12Layout->getBindingMappingLUT( setIndex ) );
        return _setPool[_poolIndex++].get();
    } else
    {
        // New
        auto newSet = Memory::makeOwned<DX12DescriptorSet>(
            _device, viewInfo, samplerInfo, dx12Layout->getBindingMappingLUT( setIndex ) );
        IDescriptorSet* ret = newSet.get();
        _setPool.push_back( std::move( newSet ) );
        _poolIndex++;
        return ret;
    }
}

void DX12DescriptorAllocator::reset() {
    _viewHeap.reset();
    _currentViewOffset = _persistentViewOffset;

    _samplerHeap.reset();
    _currentSamplerOffset = _persistentSamplerOffset;

    _poolIndex = _persistentPoolIndex;
}

StringView DX12DescriptorAllocator::getDebugName() const {
    return _desc.debugName;
}

STLW::String DX12DescriptorAllocator::toString() const {
    return STLW::String();
}

void DX12DescriptorAllocator::setDebugName( StringView name ) {
    _desc.debugName = name;
    char buffer[128];
    snprintf( buffer, sizeof( buffer ), "%.*s | Views Heap", (int)name.size(), name.data() );
    _viewHeap.setDebugName( buffer );
    snprintf( buffer, sizeof( buffer ), "%.*s | Samplers Heap", (int)name.size(), name.data() );
    _samplerHeap.setDebugName( buffer );
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

void DX12DescriptorAllocator::lockPersistent() {
    _persistentViewOffset    = _currentViewOffset;
    _persistentSamplerOffset = _currentSamplerOffset;
    _persistentPoolIndex     = _poolIndex;
}

void DX12DescriptorAllocator::unlockPersistent() {
    _persistentViewOffset    = 0;
    _persistentSamplerOffset = 0;
    _persistentPoolIndex     = 0;
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END