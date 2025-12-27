#pragma once
#include "DX12Resource.hpp"
#include "DX12Debug.hpp"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

#pragma region Texture
DX12Texture::DX12Texture( const TextureDesc& desc, DX12Device::Context& ctx, const void* initialData )
    : _desc( desc )
    , _stateTracker( desc.mipLevels, desc.arraySize ) {
    D3D12_RESOURCE_DESC dx12Desc = {};
    dx12Desc.Width               = desc.size.width;
    dx12Desc.Height              = desc.size.height;
    dx12Desc.MipLevels           = static_cast<UINT16>( desc.mipLevels );
    dx12Desc.DepthOrArraySize    = ( desc.dimension == TextureDimension::Texture3D ) ? (ushort)desc.size.depth : (ushort)desc.arraySize;
    dx12Desc.Format              = DX12Translator::get( desc.format );
    dx12Desc.SampleDesc.Count    = 1;
    dx12Desc.Flags               = D3D12_RESOURCE_FLAG_NONE;

    switch ( desc.dimension )
    {
        case TextureDimension::Texture1D:
            dx12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            break;
        case TextureDimension::Texture2D:
            dx12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            break;
        case TextureDimension::Texture3D:
            dx12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            break;
    }

    if ( desc.viewFlags & TextureViewRenderTarget )
        dx12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if ( desc.viewFlags & TextureViewDepthStencil )
        dx12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if ( desc.viewFlags & TextureViewUnorderedAccess )
        dx12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ResourceState           initialState = DX12Translator::getInitialState( desc.viewFlags );
    CD3DX12_HEAP_PROPERTIES heapProps    = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT );

    // Clearing
    D3D12_CLEAR_VALUE  clearVal  = {};
    D3D12_CLEAR_VALUE* pClearVal = nullptr;

    bool canHaveClearValue = ( dx12Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ) ||
                             ( dx12Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
    if ( canHaveClearValue )
    {
        clearVal.Format = dx12Desc.Format;
        if ( desc.viewFlags & TextureViewDepthStencil )
        {
            clearVal.DepthStencil.Depth   = desc.clearValue.depth;
            clearVal.DepthStencil.Stencil = desc.clearValue.stencil;
        } else
        {
            memcpy( clearVal.Color, &desc.clearValue.color, sizeof( float ) * 4 );
        }
        pClearVal = &clearVal;
    }

    DX_CHECK( ctx.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &dx12Desc,
        DX12Translator::get( initialState ),
        pClearVal,
        IID_PPV_ARGS( &_resource ) ) );

    _stateTracker.setState( initialState );

    createViews( ctx, true );
    setDebugName( desc.debugName );

    if ( initialData )
        uploadInitialData( ctx, initialData );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Texture created [{}]", _desc.debugName );
}

DX12Texture::DX12Texture( const ComPtr<ID3D12Resource>& resource,
                          const TextureDesc&            desc,
                          DX12Device::Context&          ctx,
                          bool                          useDecriptionParams )
    : _desc( desc )
    , _resource( resource )
    , _stateTracker( desc.mipLevels, desc.arraySize ) {
    createViews( ctx, useDecriptionParams );
    setDebugName( _desc.debugName );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Texture created [{}]", _desc.debugName );
}

DX12Texture::~DX12Texture() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Texture [{}]", _desc.debugName );
}

const TextureDesc& DX12Texture::getDescription() const {
    return _desc;
}

void DX12Texture::createViews( DX12Device::Context& ctx, bool useDescriptionParams ) {
    if ( _desc.viewFlags & TextureViewShaderResource )
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format                          = DX12Translator::get( _desc.format );
        srvDesc.ViewDimension                   = DX12Translator::getSRVDimension( _desc.dimension ); // helper
        srvDesc.Texture2D.MipLevels             = _desc.mipLevels;

        _srvHandle = ctx.heapSRV.allocateCPU();
        ctx.device->CreateShaderResourceView( _resource.Get(), useDescriptionParams ? &srvDesc : nullptr, _srvHandle );
    }

    if ( _desc.viewFlags & TextureViewRenderTarget )
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format                        = DX12Translator::get( _desc.format );
        rtvDesc.ViewDimension                 = DX12Translator::getRTVDimension( _desc.dimension );
        _rtvHandle                            = ctx.heapRTV.allocateCPU();
        ctx.device->CreateRenderTargetView( _resource.Get(), useDescriptionParams ? &rtvDesc : nullptr, _rtvHandle );
    }

    if ( _desc.viewFlags & TextureViewDepthStencil )
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format                        = DX12Translator::get( _desc.format );
        dsvDesc.ViewDimension                 = DX12Translator::getDSVDimension( _desc.dimension );
        _dsvHandle                            = ctx.heapDSV.allocateCPU();
        ctx.device->CreateDepthStencilView( _resource.Get(), useDescriptionParams ? &dsvDesc : nullptr, _dsvHandle );
    }
    if ( _desc.viewFlags & TextureViewUnorderedAccess )
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format                           = DX12Translator::get( _desc.format );
        uavDesc.ViewDimension                    = DX12Translator::getUAVDimension( _desc.dimension );
        switch ( uavDesc.ViewDimension )
        {
            case D3D12_UAV_DIMENSION_TEXTURE2D:
                uavDesc.Texture2D.MipSlice   = 0;
                uavDesc.Texture2D.PlaneSlice = 0;
                break;

            case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                uavDesc.Texture2DArray.MipSlice        = 0;
                uavDesc.Texture2DArray.FirstArraySlice = 0;
                uavDesc.Texture2DArray.ArraySize       = _desc.arraySize;
                break;

            case D3D12_UAV_DIMENSION_TEXTURE3D:
                uavDesc.Texture3D.MipSlice    = 0;
                uavDesc.Texture3D.FirstWSlice = 0;
                uavDesc.Texture3D.WSize       = -1;
                break;

            default:
                break;
        }

        _uavHandle = ctx.heapSRV.allocateCPU();

        ctx.device->CreateUnorderedAccessView(
            _resource.Get(),
            nullptr,
            useDescriptionParams ? &uavDesc : nullptr,
            _uavHandle );
    }
}

void DX12Texture::uploadInitialData( DX12Device::Context& ctx, const void* initialData ) {

    size_t bufferSize = _desc.size.width * _desc.size.height * _desc.size.depth * getFormatBytes( _desc.format );
    // Calculate required footprint size
    D3D12_RESOURCE_DESC                texDesc = _resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT64                             totalBytes = 0;
    UINT                               numRows;
    UINT64                             rowSizeInBytes;

    ctx.device->GetCopyableFootprints(
        &texDesc,
        0,
        1,
        0,
        &footprint,
        &numRows,
        &rowSizeInBytes,
        &totalBytes );

    DX12Buffer staging(
        DX12Buffer::Description {
            .size       = totalBytes,
            .memoryType = MemoryUsage::CPUVisible,
            .viewFlags  = BufferViewNone,
            .debugName  = _desc.debugName + " Staging" },
        ctx );

    // Copy initial data into padded upload memory
    uchar*       mapped      = (uchar*)staging.map();
    size_t       pixelStride = getFormatBytes( _desc.format );
    const uchar* src         = (const uchar*)initialData;

    for ( uint row = 0; row < numRows; ++row )
    {
        memcpy(
            mapped + row * footprint.Footprint.RowPitch,
            src + row * _desc.size.width * pixelStride,
            _desc.size.width * pixelStride );
    }
    staging.unmap();

    ResourceState firstUseState = _stateTracker.getCurrentState();

    // --- Upload via one-time submit ---
    ctx.uploadContext.oneTimeSubmitRaw( ctx.primaryQueue, [&]( const ComPtr<ID3D12GraphicsCommandList>& cmd ) {
        // Transition staging buffer to COPY_SOURCE
        CD3DX12_RESOURCE_BARRIER barrierStaging = CD3DX12_RESOURCE_BARRIER::Transition(
            staging.getNativeObject( ObjectTypes::DX12_Resource ),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_COPY_SOURCE );
        cmd->ResourceBarrier( 1, &barrierStaging );

        // Transition texture to COPY_DEST
        CD3DX12_RESOURCE_BARRIER barrierTex = CD3DX12_RESOURCE_BARRIER::Transition(
            _resource.Get(),
            DX12Translator::get( _stateTracker.getCurrentState() ),
            D3D12_RESOURCE_STATE_COPY_DEST );
        cmd->ResourceBarrier( 1, &barrierTex );

        // Copy buffer -> texture
        CD3DX12_TEXTURE_COPY_LOCATION dst( _resource.Get(), 0 );
        CD3DX12_TEXTURE_COPY_LOCATION src( staging.getNativeObject( ObjectTypes::DX12_Resource ), footprint );
        cmd->CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );

        // Transition texture back to first use
        CD3DX12_RESOURCE_BARRIER barrierBack = CD3DX12_RESOURCE_BARRIER::Transition(
            _resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            DX12Translator::get( firstUseState ) );
        cmd->ResourceBarrier( 1, &barrierBack );
    } );

    // Staging buffer deleted automatically via RAII
}

void DX12Texture::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _resource->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

const std::string& DX12Texture::getDebugName() const {
    return _desc.debugName;
}

NativeObject DX12Texture::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Resource:
            return NativeObject( objectType, _resource.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Texture | Wrong Object Type" );
            return nullptr;
    }
}

ResourceStateTracker& DX12Texture::stateTracker() {
    return _stateTracker;
}

std::string DX12Texture::toString() const {
    return std::string();
}
#pragma endregion
#pragma region Buffer

DX12Buffer::DX12Buffer( const BufferDesc&    desc,
                        DX12Device::Context& ctx,
                        const void*          initialData )
    : _desc( desc ) {

    ResourceState         initialState;
    D3D12_HEAP_PROPERTIES heapProps {};
    switch ( desc.memoryType )
    {
        case MemoryUsage::CPUVisible:
            initialState = ResourceState::GeneralRead;
            heapProps    = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
            break;
        case MemoryUsage::Readback:
            initialState = ResourceState::CopyDest;
            heapProps    = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_READBACK );
            break;
        default:
            if ( ( desc.usageFlags & BufferUsage::AccelerationStructure ) != BufferUsage::None )
                initialState = ResourceState::RaytracingAS;
            else
                initialState = ResourceState::Common;
            heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT );
            break;
    }

    _stateTracker.setState( initialState );

    // Resource flags (for UAV or AS)
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if ( ( desc.viewFlags & BufferViewUnorderedAccess ) != BufferViewNone ||
         ( desc.usageFlags & BufferUsage::AccelerationStructure ) != BufferUsage::None )
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer( desc.size, flags );

    D3D12_RESOURCE_STATES dxInitState = DX12Translator::get( initialState );
    DX_CHECK( ctx.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        dxInitState,
        nullptr,
        IID_PPV_ARGS( &_resource ) ) );

    setDebugName( desc.debugName );

    if ( initialData )
        uploadInitialData( ctx, initialData );

    createViews( ctx );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Buffer created [{}]", _desc.debugName );
}

void* DX12Buffer::map() {
    AXION_LOG_ASSERT(
        _desc.memoryType == MemoryUsage::CPUVisible || _desc.memoryType == MemoryUsage::Readback,
        Logger::Module::RHI,
        "Map called on non-CPU buffer" );

    if ( _mappedPtr )
        return _mappedPtr;

    CD3DX12_RANGE range( 0, 0 );
    HRESULT       hr = _resource->Map( 0, &range, &_mappedPtr );
    DX_CHECK( hr );

    return _mappedPtr;
}

void DX12Buffer::unmap() {
    if ( _mappedPtr )
    {
        _resource->Unmap( 0, nullptr );
        _mappedPtr = nullptr;
    }
}

void DX12Buffer::copyData( const void* data, ulong size, ulong offset ) {
    AXION_LOG_ASSERT( offset + size <= _desc.size, Logger::Module::RHI, "Buffer [{}] overflow!", _desc.debugName );

    bool   alreadyMapped = ( _mappedPtr != nullptr );
    uchar* dstPtr        = static_cast<uchar*>( this->map() );

    if ( dstPtr )
    {
        std::memcpy( dstPtr + offset, data, size );
        if ( !alreadyMapped )
            this->unmap();
    }
}

void* DX12Buffer::getData() const {
    return _mappedPtr;
}

void DX12Buffer::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _resource->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

NativeObject DX12Buffer::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Resource:
            return NativeObject( objectType, _resource.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Buffer | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12Buffer::toString() const {
    return fmt::format( "" );
}
D3D12_VERTEX_BUFFER_VIEW DX12Buffer::getVBV() const {
    AXION_LOG_ASSERT( _desc.usageFlags == BufferUsage::Vertex, Logger::Module::RHI, "Buffer Usage is not Vertex" );
    return _vbv;
}
D3D12_INDEX_BUFFER_VIEW DX12Buffer::getIBV() const {
    AXION_LOG_ASSERT( _desc.usageFlags == BufferUsage::Index, Logger::Module::RHI, "Buffer Usage is not Index" );
    return _ibv;
}
void DX12Buffer::createViews( DX12Device::Context& ctx ) {
    // Constant Buffer
    if ( ( _desc.viewFlags & BufferViewConstantBuffer ) != BufferViewNone )
    {

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv {};
        cbv.BufferLocation = _resource->GetGPUVirtualAddress();
        cbv.SizeInBytes    = (UINT)Helpers::alignUp( _desc.size, (size_t)256 );

        _cbvHandle = ctx.heapSRV.allocateCPU();
        ctx.device->CreateConstantBufferView( &cbv, _cbvHandle );
    }

    // SRV
    if ( ( _desc.viewFlags & BufferViewShaderResource ) != BufferViewNone )
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        // 1. Structured Buffer
        if ( _desc.stride > 0 )
        {
            srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.NumElements         = (UINT)( _desc.size / _desc.stride );
            srvDesc.Buffer.StructureByteStride = _desc.stride;
        }
        // 2. Raw Buffer (ByteAddressBuffer)
        else if ( _desc.allowRawViews )
        {
            srvDesc.Format                     = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.Buffer.NumElements         = (UINT)( _desc.size / 4 );
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_RAW;
        }
        // 3. Typed Buffer (Fallback)
        else
        {
            srvDesc.Format                     = DXGI_FORMAT_R32_UINT;
            srvDesc.Buffer.NumElements         = _desc.size / 4;
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
        }

        _srvHandle = ctx.heapSRV.allocateCPU();
        ctx.device->CreateShaderResourceView( _resource.Get(), &srvDesc, _srvHandle );
    }

    // UAV
    if ( ( _desc.viewFlags & BufferViewUnorderedAccess ) != BufferViewNone )
    {

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc {};
        desc.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
        desc.Format                     = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.NumElements         = (UINT)( _desc.size / _desc.stride );
        desc.Buffer.StructureByteStride = _desc.stride;

        _uavHandle = ctx.heapSRV.allocateCPU();
        ctx.device->CreateUnorderedAccessView( _resource.Get(), nullptr, &desc, _uavHandle );
    }
    // Special case for VBO/(IBO)
    if ( _desc.usageFlags == BufferUsage::Index )
    {
        _ibv.BufferLocation = _resource->GetGPUVirtualAddress();
        _ibv.SizeInBytes    = (UINT)_desc.size;
        _ibv.Format         = DXGI_FORMAT_R32_UINT;
    }
    if ( _desc.usageFlags == BufferUsage::Vertex )
    {
        _vbv.BufferLocation = _resource->GetGPUVirtualAddress();
        _vbv.SizeInBytes    = (UINT)_desc.size;
        _vbv.StrideInBytes  = _desc.stride;
    }
}

void DX12Buffer::uploadInitialData( DX12Device::Context& ctx, const void* initialData ) {
    // --- Fast path: CPU-visible buffer (map+copy) ---
    if ( _desc.memoryType == MemoryUsage::CPUVisible )
    {
        void* dst = map();
        std::memcpy( dst, initialData, _desc.size );
        unmap();
    }

    // --- Create staging (upload) buffer (still as a DX12Buffer RHI object) ---
    // MemoryUsage::CPUVisible means upload heap here
    DX12Buffer staging( DX12Buffer::Description {
                            .size       = _desc.size,
                            .memoryType = MemoryUsage::CPUVisible,
                            .viewFlags  = BufferViewNone,
                            .debugName  = _desc.debugName + " Staging" },
                        ctx );

    // Map & copy into staging
    {
        void* mapped = staging.map();
        std::memcpy( mapped, initialData, _desc.size );
        staging.unmap();
    }

    ctx.uploadContext.oneTimeSubmitRaw( ctx.primaryQueue, [&]( const ComPtr<ID3D12GraphicsCommandList>& cmd ) {
        // --- Transition staging buffer to COPY_SOURCE ---
        ResourceState currentStagingState = staging.stateTracker().getCurrentState();
        if ( currentStagingState != ResourceState::CopySource )
        {
            CD3DX12_RESOURCE_BARRIER bSrc = CD3DX12_RESOURCE_BARRIER::Transition(
                staging.getNativeObject( ObjectTypes::DX12_Resource ),
                DX12Translator::get( currentStagingState ),
                D3D12_RESOURCE_STATE_COPY_SOURCE );
            cmd->ResourceBarrier( 1, &bSrc );
            staging.stateTracker().setState( ResourceState::CopySource );
        }

        // --- Transition destination buffer to COPY_DEST ---
        ResourceState currentDstState = _stateTracker.getCurrentState();
        if ( currentDstState != ResourceState::CopyDest )
        {
            CD3DX12_RESOURCE_BARRIER bDst = CD3DX12_RESOURCE_BARRIER::Transition(
                _resource.Get(),
                DX12Translator::get( currentDstState ),
                D3D12_RESOURCE_STATE_COPY_DEST );
            cmd->ResourceBarrier( 1, &bDst );
            _stateTracker.setState( ResourceState::CopyDest );
        }

        // --- Issue the copy ---
        cmd->CopyBufferRegion(
            _resource.Get(),
            0,
            staging.getNativeObject( ObjectTypes::DX12_Resource ),
            0,
            _desc.size );

        // --- Optionally transition dst back to previous or desired first-use state ---
        if ( _desc.memoryType == MemoryUsage::GPUOnly )
        {
            ResourceState firstUseState = ResourceState::Common; // or infer from usage flags
            if ( _stateTracker.getCurrentState() != firstUseState )
            {
                CD3DX12_RESOURCE_BARRIER bBack = CD3DX12_RESOURCE_BARRIER::Transition(
                    _resource.Get(),
                    DX12Translator::get( _stateTracker.getCurrentState() ),
                    DX12Translator::get( firstUseState ) );
                cmd->ResourceBarrier( 1, &bBack );
                _stateTracker.setState( firstUseState );
            }
        }
    } );

    // Staging buffer will automatically be deleted due to RAII COM ptrs
}

DX12Buffer::~DX12Buffer() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Buffer [{}]", _desc.debugName );
}
#pragma endregion
#pragma region Sampler

DX12Sampler::DX12Sampler( const SamplerDesc& desc, DX12Device::Context& ctx )
    : _desc( desc ) {
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Sampler Created [{}]", _desc.debugName );

    _samplerHandle = ctx.heapSamplers.allocateCPU();

    D3D12_SAMPLER_DESC dxDesc = {};
    dxDesc.Filter             = DX12Translator::get( desc.minFilter, desc.magFilter, desc.mipFilter );
    dxDesc.AddressU           = DX12Translator::get( desc.addressU );
    dxDesc.AddressV           = DX12Translator::get( desc.addressV );
    dxDesc.AddressW           = DX12Translator::get( desc.addressW );
    dxDesc.MipLODBias         = desc.mipLODBias;
    dxDesc.MaxAnisotropy      = desc.maxAnisotropy;
    dxDesc.ComparisonFunc     = DX12Translator::get( desc.compareOp );
    dxDesc.MinLOD             = desc.minLOD;
    dxDesc.MaxLOD             = desc.maxLOD;

    ctx.device->CreateSampler( &dxDesc, _samplerHandle );

    setDebugName( desc.debugName );
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Sampler Created [{}]", _desc.debugName );
}

DX12Sampler::~DX12Sampler() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Sampler [{}]", _desc.debugName );
}

void DX12Sampler::setDebugName( const std::string& name ) {
    _desc.debugName = name;
}

NativeObject DX12Sampler::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Sampler | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12Sampler::toString() const {
    return std::string();
}

#pragma endregion
#pragma region Accel

DX12Accel::DX12Accel( const AccelDesc& desc, DX12Device::Context& ctx, bool immediateBuild )
    : _desc( desc ) {

    ComPtr<ID3D12Device5> device5;
    ctx.device->QueryInterface( IID_PPV_ARGS( &device5 ) );

    // 1. SETUP BUILD INPUTS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>          nativeGeoms;
    prepareInputs( desc, inputs, nativeGeoms );

    // 2. QUERY MEMORY REQUIREMENTS (GetPrebuildInfo)
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
    device5->GetRaytracingAccelerationStructurePrebuildInfo( &inputs, &prebuildInfo );
    _updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;
    _buildScratchSize  = prebuildInfo.ScratchDataSizeInBytes;

    // 3. ALLOCATE BUFFERS
    // A. Result Buffer: This is the persistent AS resource
    _buffer = NEW_U( DX12Buffer )( BufferDesc {
                                       .size       = ALIGN( prebuildInfo.ResultDataMaxSizeInBytes, 256 ),
                                       .memoryType = MemoryUsage::GPUOnly,
                                       .usageFlags = BufferUsage::AccelerationStructure,
                                       .viewFlags  = BufferViewUnorderedAccess,
                                       .debugName  = _desc.debugName + " Buffer" },
                                   ctx );

    if ( desc.type == AccelType::TopLevel )
        createView( ctx );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Acceleration Structure Created [{}]", _desc.debugName );

    if ( !immediateBuild )
    {
        _isBuilt = false;
        return;
    }

    // Scratch buffer
    DX12Buffer scratchBuffer( BufferDesc {
                                  .size       = prebuildInfo.ScratchDataSizeInBytes,
                                  .memoryType = MemoryUsage::GPUOnly,
                                  .usageFlags = BufferUsage::Storage,
                                  .viewFlags  = BufferViewUnorderedAccess,
                                  .debugName  = _desc.debugName + " Scratch Buffer" },
                              ctx );

    // 4. PREPARE INSTANCE DATA (TLAS ONLY)
    // TLAS build requires instances to be in a GPU buffer.
    std::unique_ptr<DX12Buffer> instancesBuffer;

    if ( desc.type == AccelType::TopLevel && !desc.instances.empty() )
    {
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> rawInstances;
        rawInstances.reserve( desc.instances.size() );

        for ( const auto& inst : desc.instances )
            rawInstances.push_back( DX12Translator::get( inst ) );

        // Upload this data to GPU.
        // Assuming CreateBufferFromData creates a buffer on Default Heap and handles upload internally
        // or creates an Upload Heap buffer directly. State must be generic read.
        instancesBuffer = NEW_U( DX12Buffer )( BufferDesc {
                                                   .size       = rawInstances.size() * sizeof( D3D12_RAYTRACING_INSTANCE_DESC ),
                                                   .memoryType = MemoryUsage::GPUOnly,
                                                   .usageFlags = BufferUsage::None,
                                                   .viewFlags  = BufferViewNone,
                                                   .debugName  = _desc.debugName + " TLAS Instances Buffer" },
                                               ctx,
                                               rawInstances.data() );

        // Point the input struct to the GPU address of the instances
        inputs.InstanceDescs = instancesBuffer->getDeviceAddress();
    }

    // 5. SETUP BUILD DESCRIPTOR
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs                                             = inputs;
    buildDesc.DestAccelerationStructureData                      = _buffer->getDeviceAddress();
    buildDesc.ScratchAccelerationStructureData                   = scratchBuffer.getDeviceAddress();

    // 6. EXECUTE COMMANDS
    ctx.uploadContext.oneTimeSubmitRaw( ctx.primaryQueue, [&]( const ComPtr<ID3D12GraphicsCommandList>& cmd ) {
        ComPtr<ID3D12GraphicsCommandList4> cmd4;
        cmd->QueryInterface( IID_PPV_ARGS( &cmd4 ) );

        cmd4->BuildRaytracingAccelerationStructure( &buildDesc, 0, nullptr );

        // 7. SYNCHRONIZATION BARRIER
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV( _buffer->getNativeObject( ObjectTypes::DX12_Resource ) );
        cmd4->ResourceBarrier( 1, &uavBarrier );
    } );

    _isBuilt = true;

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Acceleration Structure Built [{}]", _desc.debugName );
}

DX12Accel::~DX12Accel() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Acceleration Structure [{}]", _desc.debugName );
}
void DX12Accel::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _buffer->setDebugName( name + " Buffer" );
}
NativeObject DX12Accel::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Resource:
            return _buffer->getNativeObject( ObjectTypes::DX12_Resource );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Acceleration Structure | Wrong Object Type" );
            return nullptr;
    }
}
std::string DX12Accel::toString() const {
    return std::string();
}
AccelType DX12Accel::getType() const {
    return _desc.type;
}
ulong DX12Accel::getDeviceAddress() const {
    return _buffer->getDeviceAddress();
}
ulong DX12Accel::getUpdateScratchSize() const {
    return _updateScratchSize;
}
ulong DX12Accel::getBuildScratchSize() const {
    return _buildScratchSize;
}
void DX12Accel::prepareInputs( const AccelDesc&                                      desc,
                               D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& outInputs,
                               std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>&          outGeoms ) {
    outInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    // Map build flags
    outInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    if ( desc.flags & ASBuildPreferFastTrace )
        outInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    if ( desc.flags & ASBuildAllowUpdate )
        outInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    if ( desc.flags & ASBuildPreferFastBuild )
        outInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

    if ( desc.type == AccelType::BottomLevel )
    {
        outInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        outGeoms.reserve( desc.geometries.size() );
        for ( const auto& g : desc.geometries )
            outGeoms.push_back( DX12Translator::get( g ) );

        outInputs.pGeometryDescs = outGeoms.data();
        outInputs.NumDescs       = (UINT)outGeoms.size();

    } else // TopLevel
    {
        outInputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        outInputs.NumDescs = (UINT)desc.instances.size();
        // Instance data is provided via GPU buffer later, not here.
    }
}
void DX12Accel::createView( DX12Device::Context& ctx ) {

    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    desc.Format                                   = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    desc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.RaytracingAccelerationStructure.Location = _buffer->getDeviceAddress();

    _srvHandle = ctx.heapSRV.allocateCPU();

    ctx.device->CreateShaderResourceView( nullptr, &desc, _srvHandle );
}
#pragma endregion
} // namespace Graphics::RHI

AXION_NAMESPACE_END