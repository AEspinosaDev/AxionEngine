#include "DX12CommandList.hpp"
#include "DX12Debug.hpp"
#include "DX12Pipeline.hpp"
#include "DX12Resource.hpp"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {
DX12CommandList::DX12CommandList( const ComPtr<ID3D12Device2>& device, const CommandListDesc& desc )
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

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Command List [{}] created", _desc.debugName );
}

DX12CommandList::~DX12CommandList() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Command List [{}]", _desc.debugName );
}

void DX12CommandList::begin() {
    _currentHeap    = nullptr;
    _bindPoint      = PipelineBindPoint::None;
    auto& allocator = _cmdAllocators[_currentFrame];
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

void DX12CommandList::clearTexture( ITexture* texture, const ClearValue& clearValue ) {
    auto*              dxTex = static_cast<DX12Texture*>( texture );
    const TextureDesc& desc  = dxTex->getDescription();

    // Clear RenderTarget
    if ( desc.viewFlags & TextureViewRenderTarget )
    {
        barrier( texture, ResourceState::RenderTarget );
        _cmdList->ClearRenderTargetView( dxTex->getRTV(), &clearValue.color.x, 0, nullptr );
        return;
    }

    // Clear DepthStencil
    if ( desc.viewFlags & TextureViewDepthStencil )
    {
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

void DX12CommandList::copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset, ulong srcOffset ) {
    barrier( dst, ResourceState::CopyDest );
    barrier( src, ResourceState::CopySource );

    _cmdList->CopyBufferRegion(
        dst->getNativeObject( ObjectTypes::DX12_Resource ), dstOffset, src->getNativeObject( ObjectTypes::DX12_Resource ), srcOffset, numBytes );
}

void DX12CommandList::copyTexture( ITexture* dst, ITexture* src ) {
    barrier( src, Graphics::RHI::ResourceState::CopySource );
    barrier( dst, Graphics::RHI::ResourceState::CopyDest );

    _cmdList->CopyResource( dst->getNativeObject( ObjectTypes::DX12_Resource ), src->getNativeObject( ObjectTypes::DX12_Resource ) );
}

void DX12CommandList::bindComputePipeline( IComputePipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Compute Pipeline" );

    auto* dxPipeline = static_cast<DX12ComputePipeline*>( pipeline );
    _cmdList->SetPipelineState( dxPipeline->getNativeObject( ObjectTypes::DX12_PipelineState ) );

    _cmdList->SetComputeRootSignature( dxPipeline->getNativeObject( ObjectTypes::DX12_RootSignature ) );

    // 4. Actualizar estado interno (Para saber qué hacer en bindDescriptorSet)

    _bindPoint = PipelineBindPoint::Compute;
}

void DX12CommandList::bindGraphicPipeline( IGraphicPipeline* pipeline ) {
    AXION_LOG_ASSERT( pipeline, Logger::Module::RHI, "Binding NULL Graphic Pipeline" );

    auto* dxPipeline = static_cast<DX12GraphicPipeline*>( pipeline );
    _cmdList->SetPipelineState( dxPipeline->getNativeObject( ObjectTypes::DX12_PipelineState ) );

    _cmdList->SetGraphicsRootSignature( dxPipeline->getNativeObject( ObjectTypes::DX12_RootSignature ) );

    _cmdList->IASetPrimitiveTopology( DX12Translator::getD3DTopology( pipeline->getDescription().topology ) );
    _bindPoint = PipelineBindPoint::Graphic;
}

void DX12CommandList::bindDescriptorSet( uint setIndex, IDescriptorSet* set ) {
    AXION_LOG_ASSERT( set, Logger::Module::RHI, "Binding NULL Descriptor Set" );

    auto* dxSet = static_cast<DX12DescriptorSet*>( set );

    ID3D12DescriptorHeap* setHeap = dxSet->getOwnerHeap();

    if ( _currentHeap != setHeap )
    {
        _currentHeap = setHeap;

        ID3D12DescriptorHeap* heaps[] = { _currentHeap };
        _cmdList->SetDescriptorHeaps( 1, heaps );
    }

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = dxSet->getGPUHandle();

    if ( _bindPoint == PipelineBindPoint::Compute )
        _cmdList->SetComputeRootDescriptorTable( setIndex, gpuHandle );
    else
        _cmdList->SetGraphicsRootDescriptorTable( setIndex, gpuHandle );
}

void DX12CommandList::dispatch( const Extent3D& gridSize ) {
    AXION_LOG_ASSERT( _bindPoint == PipelineBindPoint::Compute, Logger::Module::RHI, "Dispatch called without Compute Pipeline" );
    _cmdList->Dispatch( gridSize.width, gridSize.height, gridSize.depth );
}

void DX12CommandList::beginRendering( const RenderingDesc& info ) {

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;

    for ( const auto& att : info.colorAttachments )
    {
        if ( att.texture )
        {
            auto* dxTex = static_cast<DX12Texture*>( att.texture );
            rtvHandles.push_back( dxTex->getRTV() );

            if ( att.loadOp == LoadOp::Clear )
            {
                clearTexture( att.texture, att.clearValue );
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
            clearTexture( info.depthStencilAttachment.texture, info.depthStencilAttachment.clearValue );
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

void DX12CommandList::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _cmdList->SetName( std::wstring( name.begin(), name.end() ).c_str() );
    for ( size_t i = 0; i < _cmdAllocators.size(); i++ )
    {
        std::string allocName = name + "_" + std::to_string( i );
        _cmdAllocators[i]->SetName( std::wstring( allocName.begin(), allocName.end() ).c_str() );
    }
}

const std::string& DX12CommandList::getDebugName() const {
    return _desc.debugName;
}

std::string RHI::DX12CommandList::toString() const {
    return std::string();
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

} // namespace Graphics::RHI

AXION_NAMESPACE_END
