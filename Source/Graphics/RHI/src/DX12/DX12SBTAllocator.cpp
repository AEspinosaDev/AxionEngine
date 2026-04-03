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

    _buffer = Memory::makeOwned<DX12Buffer>( bufDesc, ctx );

    _buffer->map();

    _allocator.initialize( _buffer.get() );

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
    u32 rgStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sbt.rayGen.argsSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    u32 rgSize   = Helpers::alignubits( rgStride, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // B) Miss
    // Max stride
    u32 maxMissArgs = 0;
    for ( const auto& r : sbt.missGroups )
        maxMissArgs = std::max( maxMissArgs, r.argsSize );

    u32 missStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxMissArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    u32 missSize   = Helpers::alignubits( missStride * (u32)sbt.missGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // C) Hit Groups
    u32 maxHitArgs = 0;
    for ( const auto& r : sbt.hitGroups )
        maxHitArgs = std::max( maxHitArgs, r.argsSize );

    u32 hitStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxHitArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    u32 hitSize   = Helpers::alignubits( hitStride * (u32)sbt.hitGroups.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // D) Callables (Optional)
    u32 maxCallArgs = 0;
    for ( const auto& r : sbt.callables )
        maxCallArgs = std::max( maxCallArgs, r.argsSize );

    u32 callStride = Helpers::alignubits( D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxCallArgs, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT );
    u32 callSize   = Helpers::alignubits( callStride * (u32)sbt.callables.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    // -------------------------------------------------------------------------
    // 2. (Check bounds)
    // -------------------------------------------------------------------------

    u32 totalNeeded = rgSize + missSize + hitSize + callSize;

    auto memBlock = _allocator.allocate( totalNeeded, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT );

    if ( !memBlock.isValid() )
        return view;

    byte*                    pCpuDst = memBlock.cpuAddress;
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

    byte* pMissStart = pCpuDst + rgSize;
    if ( !sbt.missGroups.empty() )
    {
        for ( size_t i = 0; i < sbt.missGroups.size(); ++i )
        {
            byte* dest = pMissStart + ( i * missStride );
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

    byte* pHitStart = pMissStart + missSize;
    if ( !sbt.hitGroups.empty() )
    {
        for ( size_t i = 0; i < sbt.hitGroups.size(); ++i )
        {
            byte* dest = pHitStart + ( i * hitStride );
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

    byte* pCallStart = pHitStart + hitSize;
    if ( !sbt.callables.empty() )
    {
        for ( size_t i = 0; i < sbt.callables.size(); ++i )
        {
            byte* dest = pCallStart + ( i * callStride );
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
    _allocator.reset();
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

STLW::String DX12SBTAllocator::toString() const {
    return STLW::String();
}

void DX12SBTAllocator::setDebugName( StringView name ) {
    _desc.debugName = name;
    char buffer[128];
    snprintf( buffer, sizeof( buffer ), "%.*s _Buffer", (int)name.size(), name.data() );
    _buffer->setDebugName( buffer );
}

StringView DX12SBTAllocator::getDebugName() const {
    return _desc.debugName;
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END