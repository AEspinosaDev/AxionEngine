#include "DX12SBTAllocator.h"
#include "DX12Debug.hpp"
#include "DX12Pipeline.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DX12SBTAllocator::DX12SBTAllocator( ID3D12Device* device, const SBTAllocatorDesc& desc )
    : _desc( desc ) {
    auto heapProps  = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( desc.sizeInBytes );

    DX_CHECK( device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS( &_buffer ) ) );

    // 2.(Map Persistent)
    DX_CHECK( _buffer->Map( 0, nullptr, reinterpret_cast<void**>( &_cpuBaseAddress ) ) );
    _gpuBaseAddress = _buffer->GetGPUVirtualAddress();

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 SBT Allocator created [{}]", _desc.debugName );
}

DX12SBTAllocator::~DX12SBTAllocator() {
    if ( _buffer )
        _buffer->Unmap( 0, nullptr );
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 SBT Allocator [{}]", _desc.debugName );
}

SBT::BufferView DX12SBTAllocator::allocate( const ShaderBindingTable& sbt, IRayTracingPipeline* pip ) {
    SBT::BufferView view = {};

    if ( !pip )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "SBT Upload failed: Pipeline is null" );
        return view;
    }

    // A) RayGen
    // Stride = 32 (ID) + Args. Align to 32.
    // Size Total = Stride (1 RayGen), aligned to 64.
    uint rgStride = Helpers::alignu( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sbt.rayGen.argsSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint rgSize   = Helpers::alignu( rgStride, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // B) Miss
    // Max stride
    uint maxMissArgs = 0;
    for ( const auto& r : sbt.missGroups )
        maxMissArgs = std::max( maxMissArgs, r.argsSize );

    uint missStride = Helpers::alignu( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxMissArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint missSize   = Helpers::alignu( missStride * (uint)sbt.missGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // C) Hit Groups
    uint maxHitArgs = 0;
    for ( const auto& r : sbt.hitGroups )
        maxHitArgs = std::max( maxHitArgs, r.argsSize );

    uint hitStride = Helpers::alignu( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxHitArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint hitSize   = Helpers::alignu( hitStride * (uint)sbt.hitGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // D) Callables (Optional)
    uint maxCallArgs = 0;
    for ( const auto& r : sbt.callables )
        maxCallArgs = std::max( maxCallArgs, r.argsSize );

    uint callStride = Helpers::alignu( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxCallArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint callSize   = Helpers::alignu( callStride * (uint)sbt.callables.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // -------------------------------------------------------------------------
    // 2. (Check bounds)
    // -------------------------------------------------------------------------

    uint totalNeeded = rgSize + missSize + hitSize + callSize;

    // Alinear el offset actual del allocator a 64 bytes antes de empezar
    uint startOffset = Helpers::alignu( _currentOffset, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    AXION_LOG_ASSERT( startOffset + totalNeeded <= _desc.sizeInBytes, Logger::Module::RHI, "SBT Allocator [{}] Overflow! Needed {}, Available {}", _desc.debugName, totalNeeded, _desc.sizeInBytes - startOffset );
    _currentOffset = startOffset + totalNeeded;

    uchar*                    pCpuDst = _cpuBaseAddress + startOffset;
    D3D12_GPU_VIRTUAL_ADDRESS pGpuDst = _gpuBaseAddress + startOffset;

    // -------------------------------------------------------------------------
    // 3. (MEMCPY)
    // -------------------------------------------------------------------------

    {
        void* id = pip->getShaderIdentifier( sbt.rayGen.shaderName );
        if ( id )
            memcpy( pCpuDst, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );

        if ( sbt.rayGen.argsSize > 0 && sbt.rayGen.rootArgs )
            memcpy( pCpuDst + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, sbt.rayGen.rootArgs, sbt.rayGen.argsSize );

        view.rayGenAddress = pGpuDst;
    }

    uchar* pMissStart = pCpuDst + rgSize;
    if ( !sbt.missGroups.empty() )
    {
        for ( size_t i = 0; i < sbt.missGroups.size(); ++i )
        {
            uchar* dest = pMissStart + ( i * missStride );
            void*  id   = pip->getShaderIdentifier( sbt.missGroups[i].shaderName );

            if ( id )
                memcpy( dest, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
            if ( sbt.missGroups[i].argsSize > 0 && sbt.missGroups[i].rootArgs )
                memcpy( dest + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, sbt.missGroups[i].rootArgs, sbt.missGroups[i].argsSize );
        }

        view.missRegion.startAddress  = pGpuDst + rgSize;
        view.missRegion.sizeInBytes   = missSize;
        view.missRegion.strideInBytes = missStride;
    }

    uchar* pHitStart = pMissStart + missSize;
    if ( !sbt.hitGroups.empty() )
    {
        for ( size_t i = 0; i < sbt.hitGroups.size(); ++i )
        {
            uchar* dest = pHitStart + ( i * hitStride );
            void*  id   = pip->getShaderIdentifier( sbt.hitGroups[i].shaderName );

            if ( id )
                memcpy( dest, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
            if ( sbt.hitGroups[i].argsSize > 0 && sbt.hitGroups[i].rootArgs )
                memcpy( dest + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, sbt.hitGroups[i].rootArgs, sbt.hitGroups[i].argsSize );
        }

        view.hitRegion.startAddress  = pGpuDst + rgSize + missSize;
        view.hitRegion.sizeInBytes   = hitSize;
        view.hitRegion.strideInBytes = hitStride;
    }

    uchar* pCallStart = pHitStart + hitSize;
    if ( !sbt.callables.empty() )
    {
        for ( size_t i = 0; i < sbt.callables.size(); ++i )
        {
            uchar* dest = pCallStart + ( i * callStride );
            void*  id   = pip->getShaderIdentifier( sbt.callables[i].shaderName );

            if ( id )
                memcpy( dest, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
            if ( sbt.callables[i].argsSize > 0 && sbt.callables[i].rootArgs )
                memcpy( dest + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, sbt.callables[i].rootArgs, sbt.callables[i].argsSize );
        }

        view.callableRegion.startAddress  = pGpuDst + rgSize + missSize + hitSize;
        view.callableRegion.sizeInBytes   = callSize;
        view.callableRegion.strideInBytes = callStride;
    }

    return view;
}

void DX12SBTAllocator::reset() {
    _currentOffset = 0;
}

void DX12SBTAllocator::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _buffer->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

NativeObject DX12SBTAllocator::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Resource:
            return NativeObject( objectType, _buffer.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 SBT Allocator | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12SBTAllocator::toString() const {
    return std::string();
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END