#include "GPUResourcePool.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {

GPUResourcePool::GPUResourcePool( RHI::IDevice* device )
    : _device( device ) {
    AXION_LOG_INFO( Logger::Module::GFX, "GPU Resource Pool Initialized" );
}

GPUResourcePool::~GPUResourcePool() {
    clear();
    AXION_LOG_INFO( Logger::Module::GFX, "GPU Resource Pool Destroyed" );
}
BufferHandle GPUResourcePool::createBuffer( const RHI::BufferDesc& desc, const void* initialData ) {
    std::scoped_lock lock( _mutex );

    if ( !desc.debugName.empty() && _nameToHandle.count( desc.debugName ) )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Buffer name collision [{}]. Returning existing handle.", desc.debugName );
        return _nameToHandle[desc.debugName];
    }

    auto bufferPtr = _device->createBuffer( desc, initialData );
    if ( !bufferPtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to create buffer [{}]", desc.debugName );
        return {};
    }

    uint id = UINT32_MAX;

    // Optimización: Podrías tener una std::queue<uint> _freeIndices para evitar este bucle.
    // Para < 1000 buffers, este bucle es despreciable.
    for ( size_t i = 0; i < _buffers.size(); ++i )
    {
        if ( !_buffers[i].alive )
        {
            id = (uint)i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_buffers.size();
        _buffers.emplace_back();
    }

    auto& record = _buffers[id];
    record.ptr   = std::move( bufferPtr );
    record.name  = desc.debugName;
    record.alive = true;
    record.generation++;

    if ( !desc.debugName.empty() )
        _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Buffer ID: {} [{}]", id, desc.debugName );

    return BufferHandle { id };
}

RHI::IBuffer* GPUResourcePool::getBuffer( BufferHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _buffers.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid BufferHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _buffers[handle.id];
    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead BufferHandle" );
        return nullptr;
    }

    return record.ptr.get();
}

std::optional<BufferHandle> GPUResourcePool::findBuffer( const std::string& name ) const {
    std::scoped_lock lock( _mutex );
    auto             it = _nameToHandle.find( name );
    if ( it == _nameToHandle.end() )
        return std::nullopt;
    return it->second;
}

void GPUResourcePool::destroyBuffer( BufferHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _buffers.size() )
        return;

    auto& record = _buffers[handle.id];
    if ( !record.alive )
        return;

    if ( !record.name.empty() )
        _nameToHandle.erase( record.name );

    record.ptr = nullptr;

    record.alive = false;
    
    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Buffer [{}]", record.name );
    record.name.clear();
}

void GPUResourcePool::clear() {
    std::scoped_lock lock( _mutex );
    _buffers.clear();
    _nameToHandle.clear();
}

} // namespace Graphics
AXION_NAMESPACE_END