#include "DX12Query.h"
#include "DX12Debug.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

#pragma region Time Queries

DX12TimeQueryPool::DX12TimeQueryPool( const TimeQueryPoolDesc& desc, DX12Device::Context& ctx )
    : _desc( desc ) {

    const u32 totalQueries = _desc.capacity * _desc.numFrames;

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type                  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count                 = totalQueries;
    DX_CHECK( ctx.device->CreateQueryHeap( &heapDesc, IID_PPV_ARGS( &_heap ) ) );

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType                 = D3D12_HEAP_TYPE_READBACK;

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer( totalQueries * sizeof( u64 ) );
    DX_CHECK( ctx.allocator->CreateResource( &allocDesc, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &_allocation, IID_PPV_ARGS( &_readback ) ) );

    DX_CHECK( ctx.primaryQueue->queue->GetTimestampFrequency( &_frequency ) );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Time Query Pool created [{}]", _desc.debugName );
}

DX12TimeQueryPool::~DX12TimeQueryPool() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Time Query Pool [{}]", _desc.debugName );
}

void DX12TimeQueryPool::readResults( u32 frame, u32 count, u64* outTicks ) {
    const SIZE_T begin = getFrameBase( frame ) * sizeof( u64 );
    const SIZE_T size  = count * sizeof( u64 );

    CD3DX12_RANGE readRange( begin, begin + size );
    void*         data = nullptr;
    DX_CHECK( _readback->Map( 0, &readRange, &data ) );

    memcpy( outTicks, static_cast<byte*>( data ) + begin, size );

    CD3DX12_RANGE writtenRange( 0, 0 );
    _readback->Unmap( 0, &writtenRange );
}

STLW::String DX12TimeQueryPool::toString() const {
    return STLW::String();
}

void DX12TimeQueryPool::setDebugName( StringView name ) {
    _desc.debugName = name;
}

#pragma endregion

} // namespace Graphics::RHI

AXION_NAMESPACE_END
