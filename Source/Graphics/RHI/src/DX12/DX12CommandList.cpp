#include "DX12CommandList.hpp"
#include "DX12Debug.hpp"
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

} // namespace Graphics::RHI

AXION_NAMESPACE_END
