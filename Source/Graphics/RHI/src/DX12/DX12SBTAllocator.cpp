#include "DX12SBTAllocator.h"
#include "DX12Debug.h"
#include "DX12Pipeline.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DX12SBTAllocator::DX12SBTAllocator( const SBTAllocatorDesc& desc, DX12Device::Context& ctx )
    : _desc( desc ) {

    BufferDesc bufDesc = {};
    bufDesc.size       = desc.sizeInBytes;
    bufDesc.memoryType = MemoryUsage::CPUVisible; // Importante para SBT
    bufDesc.usageFlags = BufferUsage::None;
    bufDesc.viewFlags  = BufferViewNone;
    bufDesc.debugName  = desc.debugName + "_Buffer";

    _buffer =  std::make_unique<DX12Buffer>( bufDesc, ctx );

    _buffer->map();

    _allocator = std::make_unique<LinearAllocator>( _buffer.get() );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 SBT Allocator created [{}]", _desc.debugName );
}

DX12SBTAllocator::~DX12SBTAllocator() {
    if ( _buffer )
        _buffer->unmap();
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 SBT Allocator [{}]", _desc.debugName );
}

SBT::View DX12SBTAllocator::allocate( const ShaderBindingTable& sbt, IRayTracingPipeline* pip ) {
    SBT::View view = {};

    if ( !pip )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "SBT Upload failed: Pipeline is null" );
        return view;
    }

    // A) RayGen
    // Stride = 32 (ID) + Args. Align to 32.
    // Size Total = Stride (1 RayGen), aligned to 64.
    uint rgStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sbt.rayGen.argsSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint rgSize   = Helpers::alignubits( rgStride, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // B) Miss
    // Max stride
    uint maxMissArgs = 0;
    for ( const auto& r : sbt.missGroups )
        maxMissArgs = std::max( maxMissArgs, r.argsSize );

    uint missStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxMissArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint missSize   = Helpers::alignubits( missStride * (uint)sbt.missGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // C) Hit Groups
    uint maxHitArgs = 0;
    for ( const auto& r : sbt.hitGroups )
        maxHitArgs = std::max( maxHitArgs, r.argsSize );

    uint hitStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxHitArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint hitSize   = Helpers::alignubits( hitStride * (uint)sbt.hitGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // D) Callables (Optional)
    uint maxCallArgs = 0;
    for ( const auto& r : sbt.callables )
        maxCallArgs = std::max( maxCallArgs, r.argsSize );

    uint callStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxCallArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    uint callSize   = Helpers::alignubits( callStride * (uint)sbt.callables.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // -------------------------------------------------------------------------
    // 2. (Check bounds)
    // -------------------------------------------------------------------------

    uint totalNeeded = rgSize + missSize + hitSize + callSize;

    auto memBlock = _allocator->allocate( totalNeeded, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    if ( !memBlock.isValid() )
        return view;

    uchar*                    pCpuDst = memBlock.cpuAddress;
    D3D12_GPU_VIRTUAL_ADDRESS pGpuDst = memBlock.gpuAddress;

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
    if ( _allocator )
        _allocator->reset();
}

void DX12SBTAllocator::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _buffer->setDebugName( name + "_Buffer" );
}

NativeObject DX12SBTAllocator::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Resource:
            return NativeObject( objectType, _buffer->getNativeObject( ObjectTypes::DX12_Resource ) );
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