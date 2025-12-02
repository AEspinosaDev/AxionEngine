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

    DX_CHECK( ctx.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &dx12Desc,
        DX12Translator::get( initialState ),
        nullptr,
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
    D3D12_RESOURCE_DESC texDesc = _resource->GetDesc();

    UINT64                             totalBytes = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
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

    // totalBytes now includes 256-byte row alignment requirement

    DX12Buffer staging(
        DX12Buffer::Description {
            .size       = totalBytes, // ✅ Use padded size
            .memoryType = MemoryUsage::CPUVisible,
            .viewFlags  = BufferViewNone },
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
    ctx.uploadContext.oneTimeSubmit( ctx.primaryQueue, [&]( const ComPtr<ID3D12GraphicsCommandList>& cmd ) {
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
        D3D12_TEXTURE_COPY_LOCATION dstLoc {};
        dstLoc.pResource            = _resource.Get();
        dstLoc.Type                 = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex     = 0;
        D3D12_RESOURCE_DESC dstDesc = _resource->GetDesc();

        D3D12_TEXTURE_COPY_LOCATION srcLoc {};
        srcLoc.pResource = staging.getNativeObject( ObjectTypes::DX12_Resource );
        srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        ctx.device->GetCopyableFootprints(
            &dstDesc,
            0,
            1,
            0,
            &srcLoc.PlacedFootprint,
            nullptr,
            nullptr,
            nullptr );

        cmd->CopyTextureRegion( &dstLoc, 0, 0, 0, &srcLoc, nullptr );

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
            initialState = ResourceState::Common;
            heapProps    = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT );
            break;
    }

    _stateTracker.setState( initialState );

    // Resource flags (for UAV)
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if ( ( desc.viewFlags & BufferViewUnorderedAccess ) != BufferViewNone )
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

    void*         ptr = nullptr;
    CD3DX12_RANGE range( 0, 0 );
    _resource->Map( 0, &range, &ptr );
    return ptr;
}

void DX12Buffer::unmap() {
    _resource->Unmap( 0, nullptr );
}

void DX12Buffer::copyData( const void* data, ulong size, ulong offset )
{
    AXION_LOG_ASSERT( offset + size <= _desc.size, Logger::Module::RHI, "Buffer [{}] overflow!", _desc.debugName );
    uchar* dstPtr = static_cast<uchar*>( this->map() );
    if ( dstPtr )
    {
        std::memcpy( dstPtr + offset, data, size );
        this->unmap();
    }
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
        D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
        desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        desc.Format                     = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.NumElements         = (UINT)( _desc.size / _desc.stride );
        desc.Buffer.StructureByteStride = _desc.stride;
        desc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        _srvHandle = ctx.heapSRV.allocateCPU();
        ctx.device->CreateShaderResourceView( _resource.Get(), &desc, _srvHandle );
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
        ctx.device->CreateUnorderedAccessView( _resource.Get(), nullptr, &desc, _srvHandle );
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

    ctx.uploadContext.oneTimeSubmit( ctx.primaryQueue, [&]( const ComPtr<ID3D12GraphicsCommandList>& cmd ) {
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

} // namespace Graphics::RHI

AXION_NAMESPACE_END