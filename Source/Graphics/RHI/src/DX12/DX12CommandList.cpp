#include "DX12CommandList.h"
#include "DX12Debug.h"
#include "DX12Pipeline.h"
#include "DX12Resource.h"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {
DX12CommandList::DX12CommandList( const ComPtr<ID3D12Device2>& device,
                                  const CommandListDesc&       desc )
    : _desc( desc ) {

    AXION_LOG_ASSERT( desc.numFrames > 0, Logger::Module::RHI, "Invalid frame number in CreateCommandList(). Must be greater than zero" );
    _cmdAllocators.resize( desc.numFrames );

    auto dx12type = DX12Translator::get( desc.queueType );
    for ( uint i = 0; i < desc.numFrames; i++ )
    {
        DX_CHECK( device->CreateCommandAllocator( dx12type, IID_PPV_ARGS( &_cmdAllocators[i] ) ) );
    }
    DX_CHECK( device->CreateCommandList( 0, dx12type, _cmdAllocators[0].Get(), nullptr, IID_PPV_ARGS( &_cmdList ) ) );

    DX_CHECK( _cmdList->Close() );

    if ( FAILED( _cmdList->QueryInterface( IID_PPV_ARGS( &_cmdList4 ) ) ) )
        _cmdList4 = nullptr;
    if ( FAILED( _cmdList->QueryInterface( IID_PPV_ARGS( &_cmdList6 ) ) ) )
        _cmdList6 = nullptr;

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Command List [{}] created", _desc.debugName );
}

DX12CommandList::~DX12CommandList() {
    if ( _cmdList4 )
    {
        _cmdList4->Release();
        _cmdList4 = nullptr;
    }
    if ( _cmdList6 )
    {
        _cmdList6->Release();
        _cmdList6 = nullptr;
    }
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Command List [{}]", _desc.debugName );
}

void DX12CommandList::begin() {
    _currentViewHeap    = nullptr;
    _currentSamplerHeap = nullptr;
    _currentLayout      = nullptr;
    _bindPoint          = PipelineBindPoint::None;
    auto& allocator     = _cmdAllocators[_currentFrame];
    DX_CHECK( allocator->Reset() );
    DX_CHECK( _cmdList->Reset( allocator.Get(), nullptr ) );
}

void DX12CommandList::end() {
    DX_CHECK( _cmdList->Close() );
}

void DX12CommandList::setCurrentFrame( uint index ) {
    AXION_LOG_ASSERT( index < _cmdAllocators.size(), Logger::Module::RHI, "Invalid frame index in setCurrentFrame()" );
    _currentFrame = index;
}

uint DX12CommandList::getCurrentFrame() const {
    return _currentFrame;
}

const CommandListDesc& DX12CommandList::getDescription() const {
    return _desc;
}

void DX12CommandList::barrier( ITexture* texture, ResourceState newState ) {
    auto& tracker = static_cast<DX12Texture*>( texture )->stateTracker();

    if ( !tracker.needsTransition( newState ) )
        return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture->getNativeObject( ObjectTypes::DX12_Resource ),
        DX12Translator::get( tracker.getCurrentState() ),
        DX12Translator::get( newState ) );

    _cmdList->ResourceBarrier( 1, &barrier );

    tracker.setState( newState );
}

void DX12CommandList::barrier( IBuffer* buffer, ResourceState newState ) {
    if ( buffer->getDescription().memoryType == MemoryUsage::CPUVisible )
        return;

    auto& tracker = static_cast<DX12Buffer*>( buffer )->stateTracker();

    if ( !tracker.needsTransition( newState ) )
        return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        buffer->getNativeObject( ObjectTypes::DX12_Resource ),
        DX12Translator::get( tracker.getCurrentState() ),
        DX12Translator::get( newState ) );

    _cmdList->ResourceBarrier( 1, &barrier );

    tracker.setState( newState );
}

void DX12CommandList::clearTexture( ITexture* texture, const ClearValue& clearValue, BarrierPolicy barrierPolicy ) {
    auto*              dxTex       = static_cast<DX12Texture*>( texture );
    const TextureDesc& desc        = dxTex->getDescription();
    bool               useBarriers = true;
    if ( barrierPolicy == BarrierPolicy::None )
        useBarriers = false;

    // Clear RenderTarget
    if ( desc.viewFlags & TextureViewRenderTarget )
    {
        if ( useBarriers )
            barrier( texture, ResourceState::RenderTarget );
        _cmdList->ClearRenderTargetView( dxTex->getRTV(), &clearValue.color.x, 0, nullptr );
        return;
    }

    // Clear DepthStencil
    if ( desc.viewFlags & TextureViewDepthStencil )
    {
        if ( useBarriers )
            barrier( texture, ResourceState::DepthWrite );
        _cmdList->ClearDepthStencilView(
            dxTex->getDSV(),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearValue.depth,
            clearValue.stencil,
            0,
            nullptr );
        return;
    }

    // Clear UAV
    if ( desc.viewFlags & TextureViewUnorderedAccess )
    {
        if ( useBarriers )
            barrier( texture, ResourceState::UnorderedAccess );
        float vals[4] = { clearValue.color.x, clearValue.color.y, clearValue.color.z, clearValue.color.w };
        // _cmdList->ClearUnorderedAccessViewFloat(
        //     dxTex->getGPUUAV(), // GPU Ptr
        //     dxTex->getCPUUAV(), // CPU Ptr
        //     dxTex->getNativeObject(ObjectTypes::DX12_Resource),
        //     vals,
        //     0,
        //     nullptr
        // );
        return;
    }
}

void DX12CommandList::copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset, ulong srcOffset, BarrierPolicy barrierPolicy ) {
    if ( barrierPolicy == BarrierPolicy::Auto )
    {
        barrier( dst, ResourceState::CopyDest );
        barrier( src, ResourceState::CopySource );
    }
    _cmdList->CopyBufferRegion(
        dst->getNativeObject( ObjectTypes::DX12_Resource ), dstOffset, src->getNativeObject( ObjectTypes::DX12_Resource ), srcOffset, numBytes );
}

void DX12CommandList::copyTexture( ITexture* dst, ITexture* src, BarrierPolicy barrierPolicy ) {
    if ( barrierPolicy == BarrierPolicy::Auto )
    {
        barrier( src, Graphics::RHI::ResourceState::CopySource );
        barrier( dst, Graphics::RHI::ResourceState::CopyDest );
    }

    _cmdList->CopyResource( dst->getNativeObject( ObjectTypes::DX12_Resource ), src->getNativeObject( ObjectTypes::DX12_Resource ) );
}

void DX12CommandList::uploadBuffer( IBuffer* dst, const void* data, ulong size, ulong dstOffset, ITransientAllocator* allocator, BarrierPolicy barrierPolicy ) {
    if ( !dst || !data || size == 0 || !allocator )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Invalid arguments for uploadBuffer" );
        return;
    }

    auto mem = allocator->allocateUpload( size, 256 );

    if ( !mem.isValid() )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Upload Heap during Buffer Upload" );
        return;
    }

    // Copy to staging buffer
    memcpy( mem.cpuAddress, data, size );

    copyBuffer( dst, mem.buffer, size, dstOffset, mem.offset, barrierPolicy );
}

void DX12CommandList::uploadTexture( ITexture* dst, const void* data, ITransientAllocator* allocator, uint mipSlice, uint arraySlice, BarrierPolicy barrierPolicy ) {
    if ( !dst || !data || !allocator )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Invalid arguments for uploadTexture" );
        return;
    }

    ID3D12Resource*     d3dRes = dst->getNativeObject( ObjectTypes::DX12_Resource );
    D3D12_RESOURCE_DESC desc   = d3dRes->GetDesc();

    ComPtr<ID3D12Device> device;
    d3dRes->GetDevice( IID_PPV_ARGS( &device ) );

    // Query footprint requirements for the specific subresource (mip + array slice)
    uint subresourceIndex = mipSlice + ( arraySlice * desc.MipLevels );

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT                               numRows;
    UINT64                             rowSizeInBytes;
    UINT64                             totalBytesRequired;

    device->GetCopyableFootprints( &desc,
                                   subresourceIndex,
                                   1,
                                   0,
                                   &footprint,
                                   &numRows,
                                   &rowSizeInBytes,
                                   &totalBytesRequired );

    auto mem = allocator->allocateUpload( totalBytesRequired, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT );

    if ( !mem.isValid() )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Upload Heap during Texture Upload" );
        return;
    }

    // Copy data to the staging buffer row by row to respect the D3D12 Pitch Alignment
    uchar*       mapped      = static_cast<uchar*>( mem.cpuAddress );
    size_t       pixelStride = getFormatBytes( dst->getDescription().format );
    const uchar* src         = static_cast<const uchar*>( data );
    uint         width       = desc.Width;

    for ( uint row = 0; row < numRows; ++row )
    {
        memcpy(
            mapped + row * footprint.Footprint.RowPitch,
            src + row * width * pixelStride,
            width * pixelStride );
    }

    if ( barrierPolicy == BarrierPolicy::Auto )
    {
        barrier( dst, Graphics::RHI::ResourceState::CopyDest );
        barrier( mem.buffer, Graphics::RHI::ResourceState::CopySource );
    }

    CD3DX12_TEXTURE_COPY_LOCATION srcLocation( mem.buffer->getNativeObject( ObjectTypes::DX12_Resource ), footprint );
    CD3DX12_TEXTURE_COPY_LOCATION dstLocation( d3dRes, subresourceIndex );
    srcLocation.PlacedFootprint.Offset += mem.offset;

    _cmdList->CopyTextureRegion( &dstLocation, 0, 0, 0, &srcLocation, nullptr );
}

void DX12CommandList::updateAccel( IAccel* accel, const AccelDesc& newDesc, ITransientAllocator* allocator ) {
    auto* dxAccel = static_cast<DX12Accel*>( accel );

    if ( !dxAccel || !allocator )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Invalid arguments for updateAccel" );
        return;
    }

    if ( !( dxAccel->getDescription().flags & ASBuildAllowUpdate ) )
    {
        AXION_LOG_WARN( Logger::Module::RHI, "AS Update requested but AllowUpdate flag not set: {}", dxAccel->getDescription().debugName );
        return;
    }

    if ( !validateUpdateCompatibility( dxAccel, newDesc ) )
        return;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    STLW::Vector<D3D12_RAYTRACING_GEOMETRY_DESC>          nativeGeoms;
    DX12Accel::prepareInputs( newDesc, inputs, nativeGeoms );

    // Force for some reason here
    inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

    if ( newDesc.type == AccelType::TopLevel )
    {
        uint instanceDataSize = (uint)( newDesc.instances.size() * sizeof( D3D12_RAYTRACING_INSTANCE_DESC ) );

        auto instanceMem = allocator->allocateUpload( instanceDataSize, 16 );

        if ( !instanceMem.isValid() )
        {
            AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Upload Heap during AS update" );
            return;
        }

        auto* dst = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>( instanceMem.cpuAddress );
        for ( size_t i = 0; i < newDesc.instances.size(); ++i )
        {
            dst[i] = DX12Translator::get( newDesc.instances[i] );
        }

        inputs.InstanceDescs = instanceMem.gpuAddress;
        inputs.NumDescs      = (UINT)newDesc.instances.size();
    }

    ulong scratchSize = dxAccel->getUpdateScratchSize();

    auto scratchMem = allocator->allocateScratch( scratchSize, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT );

    if ( !scratchMem.isValid() )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Scratch Heap during AS update" );
        return;
    }

    ID3D12Resource* resourceNative = dxAccel->getNativeObject( ObjectTypes::DX12_Resource );

    CD3DX12_RESOURCE_BARRIER barrierBefore = CD3DX12_RESOURCE_BARRIER::UAV( resourceNative );
    _cmdList4->ResourceBarrier( 1, &barrierBefore );

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs                                             = inputs;
    buildDesc.SourceAccelerationStructureData                    = dxAccel->getDeviceAddress(); // Input (Old)
    buildDesc.DestAccelerationStructureData                      = dxAccel->getDeviceAddress(); // Output (New - In Place)
    buildDesc.ScratchAccelerationStructureData                   = scratchMem.gpuAddress;

    _cmdList4->BuildRaytracingAccelerationStructure( &buildDesc, 0, nullptr );

    CD3DX12_RESOURCE_BARRIER barrierAfter = CD3DX12_RESOURCE_BARRIER::UAV( resourceNative );
    _cmdList4->ResourceBarrier( 1, &barrierAfter );
}

void DX12CommandList::buildAccel( IAccel* accel, const AccelDesc& newDesc, ITransientAllocator* allocator ) {
    auto* dxAccel = static_cast<DX12Accel*>( accel );

    if ( !dxAccel || !allocator )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Invalid arguments for buildAccel" );
        return;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    STLW::Vector<D3D12_RAYTRACING_GEOMETRY_DESC>          nativeGeoms;
    DX12Accel::prepareInputs( newDesc, inputs, nativeGeoms );

    if ( newDesc.type == AccelType::TopLevel )
    {
        uint instanceDataSize = (uint)( newDesc.instances.size() * sizeof( D3D12_RAYTRACING_INSTANCE_DESC ) );

        auto instanceMem = allocator->allocateUpload( instanceDataSize, 16 );

        if ( !instanceMem.isValid() )
        {
            AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Upload Heap during AS Build" );
            return;
        }

        auto* dst = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>( instanceMem.cpuAddress );
        for ( size_t i = 0; i < newDesc.instances.size(); ++i )
        {
            dst[i] = DX12Translator::get( newDesc.instances[i] );
        }

        inputs.InstanceDescs = instanceMem.gpuAddress;
        inputs.NumDescs      = (UINT)newDesc.instances.size();
    }

    ulong scratchSize = dxAccel->getBuildScratchSize();

    auto scratchMem = allocator->allocateScratch( scratchSize, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT );

    if ( !scratchMem.isValid() )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "OOM in Transient Scratch Heap during AS Build (Size: {} bytes)", scratchSize );
        return;
    }

    ID3D12Resource* resourceNative = dxAccel->getNativeObject( ObjectTypes::DX12_Resource );

    CD3DX12_RESOURCE_BARRIER barrierBefore = CD3DX12_RESOURCE_BARRIER::UAV( resourceNative );
    _cmdList4->ResourceBarrier( 1, &barrierBefore );

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs                                             = inputs;
    buildDesc.DestAccelerationStructureData                      = dxAccel->getDeviceAddress(); // Output
    buildDesc.SourceAccelerationStructureData                    = 0;                           // NULL -> Full Build
    buildDesc.ScratchAccelerationStructureData                   = scratchMem.gpuAddress;

    _cmdList4->BuildRaytracingAccelerationStructure( &buildDesc, 0, nullptr );

    CD3DX12_RESOURCE_BARRIER barrierAfter = CD3DX12_RESOURCE_BARRIER::UAV( resourceNative );
    _cmdList4->ResourceBarrier( 1, &barrierAfter );
}

void DX12CommandList::bindComputePipeline( IComputePipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Compute Pipeline" );

    auto* dxPipeline = static_cast<DX12ComputePipeline*>( pipeline );
    _cmdList->SetPipelineState( dxPipeline->getNativeObject( ObjectTypes::DX12_PipelineState ) );

    auto* newLayout = pipeline->getDescription().layout;

    if ( _currentLayout != newLayout || _bindPoint != PipelineBindPoint::Compute )
    {
        _currentLayout = newLayout;

        auto* dxLayout = static_cast<DX12PipelineLayout*>( newLayout );
        _cmdList->SetComputeRootSignature( static_cast<ID3D12RootSignature*>(
            dxLayout->getNativeObject( ObjectTypes::DX12_RootSignature ) ) );
    }

    _bindPoint = PipelineBindPoint::Compute;
}

void DX12CommandList::bindGraphicPipeline( IGraphicPipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Graphic Pipeline" );

    auto* dxPipeline = static_cast<DX12GraphicPipeline*>( pipeline );
    _cmdList->SetPipelineState( dxPipeline->getNativeObject( ObjectTypes::DX12_PipelineState ) );

    _cmdList->IASetPrimitiveTopology( DX12Translator::getD3DTopology( pipeline->getDescription().topology ) );

    auto* newLayout = pipeline->getDescription().layout;

    if ( _currentLayout != newLayout || _bindPoint != PipelineBindPoint::Graphic )
    {
        _currentLayout = newLayout;

        auto* dxLayout = static_cast<DX12PipelineLayout*>( newLayout );
        _cmdList->SetGraphicsRootSignature( static_cast<ID3D12RootSignature*>(
            dxLayout->getNativeObject( ObjectTypes::DX12_RootSignature ) ) );
    }

    _bindPoint = PipelineBindPoint::Graphic;
}

void DX12CommandList::bindRaytracingPipeline( IRayTracingPipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Raytracing Pipeline" );

    if ( !_cmdList4 )
        return;

    auto* dxPipeline = static_cast<DX12RayTracingPipeline*>( pipeline );
    _cmdList4->SetPipelineState1( dxPipeline->getNativeObject( ObjectTypes::DX12_StateObject ) );

    auto* newLayout = pipeline->getDescription().layout;
    if ( _currentLayout != newLayout || _bindPoint != PipelineBindPoint::RTX )
    {
        _currentLayout = newLayout;

        auto* dxLayout = static_cast<DX12PipelineLayout*>( newLayout );
        _cmdList->SetComputeRootSignature( static_cast<ID3D12RootSignature*>(
            dxLayout->getNativeObject( ObjectTypes::DX12_RootSignature ) ) );
    }

    _bindPoint = PipelineBindPoint::RTX;
}

void DX12CommandList::bindMeshPipeline( IMeshPipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Mesh Pipeline" );

    auto* dxPipeline = static_cast<DX12MeshPipeline*>( pipeline );
    _cmdList->SetPipelineState( dxPipeline->getNativeObject( ObjectTypes::DX12_PipelineState ) );

    auto* newLayout = pipeline->getDescription().layout;

    if ( _currentLayout != newLayout || _bindPoint != PipelineBindPoint::Graphic )
    {
        _currentLayout = newLayout;

        auto* dxLayout = static_cast<DX12PipelineLayout*>( newLayout );
        _cmdList->SetGraphicsRootSignature( static_cast<ID3D12RootSignature*>(
            dxLayout->getNativeObject( ObjectTypes::DX12_RootSignature ) ) );
    }

    _bindPoint = PipelineBindPoint::Graphic;
}

void DX12CommandList::bindDescriptorSet( uint setIndex, IDescriptorSet* set ) {
    AXION_LOG_ASSERT( set, Logger::Module::RHI, "Binding NULL Descriptor Set" );

    auto* dxSet = static_cast<DX12DescriptorSet*>( set );

    ID3D12DescriptorHeap* viewHeap    = dxSet->getViewOwnerHeap();
    ID3D12DescriptorHeap* samplerHeap = dxSet->getSamplerOwnerHeap();

    if ( _currentViewHeap != viewHeap || _currentSamplerHeap != samplerHeap )
    {
        _currentViewHeap    = viewHeap;
        _currentSamplerHeap = samplerHeap;

        ID3D12DescriptorHeap* heapsToBind[2] = {};
        uint                  heapCount      = 0;

        if ( _currentViewHeap )
            heapsToBind[heapCount++] = _currentViewHeap;
        if ( _currentSamplerHeap )
            heapsToBind[heapCount++] = _currentSamplerHeap;

        if ( heapCount > 0 )
            _cmdList->SetDescriptorHeaps( heapCount, heapsToBind );
    }

    auto indices = static_cast<DX12PipelineLayout*>( _currentLayout )->getRootIndices( setIndex );

    // Views
    if ( indices.first != -1 )
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = dxSet->getViewGPUHandle();

        if ( _bindPoint == PipelineBindPoint::Compute || _bindPoint == PipelineBindPoint::RTX )
            _cmdList->SetComputeRootDescriptorTable( indices.first, handle );
        else
            _cmdList->SetGraphicsRootDescriptorTable( indices.first, handle );
    }

    // Samplers
    if ( indices.second != -1 )
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = dxSet->getSamplerGPUHandle();

        if ( _bindPoint == PipelineBindPoint::Compute || _bindPoint == PipelineBindPoint::RTX )
            _cmdList->SetComputeRootDescriptorTable( indices.second, handle );
        else
            _cmdList->SetGraphicsRootDescriptorTable( indices.second, handle );
    }
}

void DX12CommandList::bindDescriptorSet( uint setIndex, IDescriptorSet* set, IPipelineLayout* layout, PipelineBindPoint bindPoint ) {
    AXION_LOG_ASSERT( set, Logger::Module::RHI, "Binding NULL Descriptor Set" );

    auto* dxSet = static_cast<DX12DescriptorSet*>( set );

    if ( _currentLayout != layout || _bindPoint != bindPoint )
    {
        _currentLayout = layout;
        _bindPoint     = bindPoint;

        auto*                dxLayout = static_cast<DX12PipelineLayout*>( layout );
        ID3D12RootSignature* rootSig  = static_cast<ID3D12RootSignature*>(
            dxLayout->getNativeObject( ObjectTypes::DX12_RootSignature ) );

        if ( _bindPoint == PipelineBindPoint::Compute || _bindPoint == PipelineBindPoint::RTX )
            _cmdList->SetComputeRootSignature( rootSig );
        else
            _cmdList->SetGraphicsRootSignature( rootSig );
    }

    ID3D12DescriptorHeap* viewHeap    = dxSet->getViewOwnerHeap();
    ID3D12DescriptorHeap* samplerHeap = dxSet->getSamplerOwnerHeap();

    if ( _currentViewHeap != viewHeap || _currentSamplerHeap != samplerHeap )
    {
        _currentViewHeap    = viewHeap;
        _currentSamplerHeap = samplerHeap;

        ID3D12DescriptorHeap* heapsToBind[2] = {};
        uint                  heapCount      = 0;

        if ( _currentViewHeap )
            heapsToBind[heapCount++] = _currentViewHeap;
        if ( _currentSamplerHeap )
            heapsToBind[heapCount++] = _currentSamplerHeap;

        if ( heapCount > 0 )
            _cmdList->SetDescriptorHeaps( heapCount, heapsToBind );
    }

    auto* dxCurrentLayout = static_cast<DX12PipelineLayout*>( _currentLayout );
    auto  indices         = dxCurrentLayout->getRootIndices( setIndex );

    // Views
    if ( indices.first != -1 )
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = dxSet->getViewGPUHandle();

        if ( _bindPoint == PipelineBindPoint::Compute || _bindPoint == PipelineBindPoint::RTX )
            _cmdList->SetComputeRootDescriptorTable( indices.first, handle );
        else
            _cmdList->SetGraphicsRootDescriptorTable( indices.first, handle );
    }

    // Samplers
    if ( indices.second != -1 )
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = dxSet->getSamplerGPUHandle();

        if ( _bindPoint == PipelineBindPoint::Compute || _bindPoint == PipelineBindPoint::RTX )
            _cmdList->SetComputeRootDescriptorTable( indices.second, handle );
        else
            _cmdList->SetGraphicsRootDescriptorTable( indices.second, handle );
    }
}

void DX12CommandList::dispatch( const Extent3D& gridSize ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::Compute, Logger::Module::RHI, "Dispatch called without Compute Pipeline" );
    _cmdList->Dispatch( gridSize.width, gridSize.height, gridSize.depth );
}

void DX12CommandList::dispatchMesh( const Extent3D& gridSize ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::Graphic, Logger::Module::RHI, "DispatchMesh called without Graphic Pipeline" );
    _cmdList6->DispatchMesh( gridSize.width, gridSize.height, gridSize.depth );
}

void DX12CommandList::dispatchRays( const SBT::View& sbtView, const Extent3D& screenSize ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::RTX, Logger::Module::RHI, "Dispatch called without Raytracing Pipeline" );
    if ( !_cmdList4 )
    {
        AXION_LOG_WARN_ONCE( Logger::Module::RHI, "Attempting DispatchRays on unsupported hardware" );
        return;
    }

    D3D12_DISPATCH_RAYS_DESC desc = {};

    // 1. Gen
    desc.RayGenerationShaderRecord.StartAddress = sbtView.rayGenAddress;
    desc.RayGenerationShaderRecord.SizeInBytes  = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    // 2. Miss Table
    desc.MissShaderTable.StartAddress  = sbtView.missRegion.startAddress;
    desc.MissShaderTable.SizeInBytes   = sbtView.missRegion.sizeInBytes;
    desc.MissShaderTable.StrideInBytes = sbtView.missRegion.strideInBytes;

    // 3. Hit Group Table
    desc.HitGroupTable.StartAddress  = sbtView.hitRegion.startAddress;
    desc.HitGroupTable.SizeInBytes   = sbtView.hitRegion.sizeInBytes;
    desc.HitGroupTable.StrideInBytes = sbtView.hitRegion.strideInBytes;

    // 4. Callable Table
    if ( sbtView.callableRegion.sizeInBytes > 0 )
    {
        desc.CallableShaderTable.StartAddress  = sbtView.callableRegion.startAddress;
        desc.CallableShaderTable.SizeInBytes   = sbtView.callableRegion.sizeInBytes;
        desc.CallableShaderTable.StrideInBytes = sbtView.callableRegion.strideInBytes;
    }

    // 5. Dimensiones (Lo que te faltaba)
    desc.Width  = screenSize.width;
    desc.Height = screenSize.height;
    desc.Depth  = screenSize.depth;

    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    _cmdList.As( &cmdList4 ); // O _cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4));

    if ( cmdList4 )
    {
        cmdList4->DispatchRays( &desc );
    } else
    {
        // Log Error: Tu dispositivo o driver no soporta DXR o falló el cast
        AXION_LOG_ERROR( Logger::Module::RHI, "Failed to cast to ID3D12GraphicsCommandList4 for DispatchRays" );
    }
}

void DX12CommandList::beginRendering( const RenderingDesc& info ) {

    STLW::Vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;

    for ( const auto& att : info.colorAttachments )
    {
        if ( att.texture )
        {
            auto* dxTex     = static_cast<DX12Texture*>( att.texture );
            auto  rtvHandle = dxTex->getRTV();
            rtvHandles.push_back( rtvHandle );

            if ( att.loadOp == LoadOp::Clear )
            {
                const float* colorPtr = nullptr;

                if ( att.clearValue.has_value() )
                {
                    colorPtr = &att.clearValue->color.x;
                } else
                {
                    colorPtr = &dxTex->getDescription().clearValue.color.x;
                }

                _cmdList->ClearRenderTargetView( rtvHandle, colorPtr, 0, nullptr );
            }
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    bool                        hasDepth  = false;
    if ( info.depthStencilAttachment.texture )
    {
        auto* dxDepth = static_cast<DX12Texture*>( info.depthStencilAttachment.texture );
        dsvHandle     = dxDepth->getDSV();
        hasDepth      = true;

        if ( info.depthStencilAttachment.loadOp == LoadOp::Clear )
        {
            float depth   = 0.0f;
            uchar stencil = 0;

            if ( info.depthStencilAttachment.clearValue.has_value() )
            {
                depth   = info.depthStencilAttachment.clearValue->depth;
                stencil = info.depthStencilAttachment.clearValue->stencil;
            } else
            {
                depth   = dxDepth->getDescription().clearValue.depth;
                stencil = dxDepth->getDescription().clearValue.stencil;
            }

            _cmdList->ClearDepthStencilView(
                dsvHandle,
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                depth,
                stencil,
                0,
                nullptr );
        }
    }

    _cmdList->OMSetRenderTargets(
        (uint)rtvHandles.size(),
        rtvHandles.data(),
        FALSE, // Descriptores contiguos? No necesariamente, pasamos array
        hasDepth ? &dsvHandle : nullptr );

    D3D12_VIEWPORT vp = {};
    vp.Width          = (float)info.renderArea.width;
    vp.Height         = (float)info.renderArea.height;
    vp.MinDepth       = 0.0f;
    vp.MaxDepth       = 1.0f;
    _cmdList->RSSetViewports( 1, &vp );

    D3D12_RECT scissor = {};
    scissor.right      = info.renderArea.width;
    scissor.bottom     = info.renderArea.height;
    _cmdList->RSSetScissorRects( 1, &scissor );
}

void DX12CommandList::endRendering() {
    // DX12 Doesnt need it
}

void DX12CommandList::draw( uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::Graphic, Logger::Module::RHI, "Draw called without Graphic Pipeline!" );
    _cmdList->DrawInstanced(
        vertexCount,
        instanceCount,
        firstVertex,
        firstInstance );
}

void DX12CommandList::drawIndexed( uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::Graphic, Logger::Module::RHI, "Draw called without Graphic Pipeline!" );
    _cmdList->DrawIndexedInstanced(
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance );
}

void DX12CommandList::bindVertexBuffer( uint slot, IBuffer* buffer ) {
    auto* dxBuf = static_cast<DX12Buffer*>( buffer );
    auto  view  = dxBuf->getVBV();
    _cmdList->IASetVertexBuffers( slot, 1, &view );
}

void DX12CommandList::bindIndexBuffer( IBuffer* buffer ) {
    auto* dxBuf = static_cast<DX12Buffer*>( buffer );
    auto  view  = dxBuf->getIBV();
    _cmdList->IASetIndexBuffer( &view );
}

void DX12CommandList::drawIndexedIndirect( IBuffer* indirectBuffer, ulong bufferOffset, uint maxDrawCount, IBuffer* countBuffer, ulong countBufferOffset ) {
    AXION_LOG_ASSERT( indirectBuffer, Logger::Module::RHI, "Indirect command buffer cannot be null" );
    AXION_LOG_ASSERT( maxDrawCount > 0, Logger::Module::RHI, "Max draw count must be > 0" );

    ID3D12Resource* dxCountResource = nullptr;
    if ( countBuffer )
    {
        dxCountResource = countBuffer->getNativeObject( ObjectTypes::DX12_Resource );

        // DEBUG: Verificar que countBuffer tiene el flag INDIRECT_ARGUMENT si tienes tracking de estados
        // AXION_ASSERT( dxCountBuffer->getState() == ResourceState::IndirectArgument );
    }

    auto* dxLayout = static_cast<DX12PipelineLayout*>( _currentLayout );
    _cmdList->ExecuteIndirect(
        dxLayout->getIndirectCommandSignature(),
        maxDrawCount,
        indirectBuffer->getNativeObject( ObjectTypes::DX12_Resource ),
        bufferOffset,
        dxCountResource,
        countBufferOffset );
}

NativeObject DX12CommandList::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_CommandList:
            return NativeObject( objectType, _cmdList.Get() );
        case ObjectTypes::DX12_CommandAllocator:
            return NativeObject( objectType, _cmdAllocators[_currentFrame].Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 CommandList | Wrong Object Type" );
            return nullptr;
    }
}

void DX12CommandList::setDebugName( std::string_view name ) {
    _desc.debugName = name;

    if ( name.empty() || !_cmdList )
        return;

    setNativeName( _cmdList.Get(), name );
    for ( uint i = 0; i < (uint)_cmdAllocators.size(); ++i )
    {
        char allocName[128];
        snprintf( allocName, sizeof( allocName ), "%.100s_Alloc_%u", name.data(), i );

        setNativeName( _cmdAllocators[i].Get(), allocName );
    }
}

std::string_view DX12CommandList::getDebugName() const {
    return _desc.debugName;
}

STLW::String RHI::DX12CommandList::toString() const {
    return STLW::String();
}

void DX12CommandList::pushConstants( uint setIndex, const void* data, uint numValues32Bit, uint offset32Bit ) {
    AXION_LOG_ASSERT( _bindPoint != PipelineBindPoint::None, Logger::Module::RHI, "Attempting to set Push Constants without a bound Pipeline!" );
    if ( _bindPoint == PipelineBindPoint::Compute )
        _cmdList->SetComputeRoot32BitConstants(
            setIndex,
            numValues32Bit,
            data,
            offset32Bit );
    else
        _cmdList->SetGraphicsRoot32BitConstants(
            setIndex,
            numValues32Bit,
            data,
            offset32Bit );
}

bool DX12CommandList::validateUpdateCompatibility( const IAccel* accel, const AccelDesc& newDesc ) {
    const auto& oldDesc = accel->getDescription();

    if ( oldDesc.type != newDesc.type )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "AS Update Failed: Type mismatch (BLAS vs TLAS)" );
        return false;
    }

    // 2. TLAS Validation: Cannot exceed initial instance capacity
    if ( newDesc.type == AccelType::TopLevel )
    {
        if ( newDesc.instances.size() > oldDesc.instances.size() )
        {
            AXION_LOG_ERROR( Logger::Module::RHI,
                             "AS Update Failed: Instance count ({}) exceeds initial capacity ({})",
                             newDesc.instances.size(),
                             oldDesc.instances.size() );
            return false;
        }
    }
    // 3. BLAS Validation: Geometry count and types must match exactly
    else
    {
        if ( newDesc.geometries.size() != oldDesc.geometries.size() )
        {
            AXION_LOG_ERROR( Logger::Module::RHI,
                             "AS Update Failed: Geometry count changed ({} vs {}). Full Rebuild required.",
                             newDesc.geometries.size(),
                             oldDesc.geometries.size() );
            return false;
        }

        for ( size_t i = 0; i < newDesc.geometries.size(); ++i )
        {
            // Check flags (e.g. Opaque) - Changing flags often requires rebuild
            // if ( newDesc.geometries[i].flags != oldDesc.geometries[i].flags )
            // {
            //     AXION_LOG_WARN( Logger::Module::RHI, "AS Update Warning: Geometry [{}] flags changed.", i );
            // }
            // Topology check (Type)
            if ( newDesc.geometries[i].primitiveType != oldDesc.geometries[i].primitiveType )
            {
                AXION_LOG_ERROR( Logger::Module::RHI, "AS Update Failed: Geometry [{}] type mismatch.", i );
                return false;
            }
        }
    }

    return true;
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END
