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
BufferHandle GPUResourcePool::createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup ) {
    std::scoped_lock lock( _mutex );

    if ( allowLookup && !desc.debugName.empty() && _buffNameToHandle.count( desc.debugName ) )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Buffer name collision [{}]. Returning existing handle.", desc.debugName );
        return _buffNameToHandle[desc.debugName];
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

    if ( !desc.debugName.empty() && allowLookup )
        _buffNameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Buffer ID: {} [{}]", id, desc.debugName );

    return BufferHandle { id };
}

TextureHandle GPUResourcePool::createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup ) {
    std::scoped_lock lock( _mutex );

    if ( allowLookup && !desc.debugName.empty() && _texNameToHandle.count( desc.debugName ) )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Texture name collision [{}]. Returning existing handle.", desc.debugName );
        return _texNameToHandle[desc.debugName];
    }

    auto texturePtr = _device->createTexture( desc, initialData );
    if ( !texturePtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to create Texture [{}]", desc.debugName );
        return {};
    }

    uint id = UINT32_MAX;

    // Optimización: Podrías tener una std::queue<uint> _freeIndices para evitar este bucle.
    // Para < 1000 buffers, este bucle es despreciable.
    for ( size_t i = 0; i < _textures.size(); ++i )
    {
        if ( !_textures[i].alive )
        {
            id = (uint)i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_textures.size();
        _textures.emplace_back();
    }

    auto& record = _textures[id];
    record.ptr   = std::move( texturePtr );
    record.name  = desc.debugName;
    record.alive = true;
    record.generation++;

    if ( allowLookup && !desc.debugName.empty() )
        _texNameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Texture ID: {} [{}]", id, desc.debugName );

    return TextureHandle { id };
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
    auto             it = _buffNameToHandle.find( name );
    if ( it == _buffNameToHandle.end() )
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
        _buffNameToHandle.erase( record.name );

    record.ptr = nullptr;

    record.alive = false;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Buffer [{}]", record.name );
    record.name.clear();
}

RHI::ITexture* GPUResourcePool::getTexture( TextureHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _textures.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid TextureHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _textures[handle.id];
    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead TextureHandle" );
        return nullptr;
    }

    return record.ptr.get();
}

std::optional<TextureHandle> GPUResourcePool::findTexture( const std::string& name ) const {
    std::scoped_lock lock( _mutex );
    auto             it = _texNameToHandle.find( name );
    if ( it == _texNameToHandle.end() )
        return std::nullopt;
    return it->second;
}

void GPUResourcePool::destroyTexture( TextureHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _textures.size() )
        return;

    auto& record = _textures[handle.id];
    if ( !record.alive )
        return;

    if ( !record.name.empty() )
        _texNameToHandle.erase( record.name );

    record.ptr = nullptr;

    record.alive = false;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Texture [{}]", record.name );
    record.name.clear();
}

TextureHandle GPUResourcePool::registerExternalTexture( RHI::ITexture* ptr, const std::string& name ) {
    std::scoped_lock lock( _mutex );

    uint id = UINT32_MAX;

    // Optimización: Podrías tener una std::queue<uint> _freeIndices para evitar este bucle.
    // Para < 1000 buffers, este bucle es despreciable.
    for ( size_t i = 0; i < _textures.size(); ++i )
    {
        if ( !_textures[i].alive )
        {
            id = (uint)i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_textures.size();
        _textures.emplace_back();
    }

    auto& record = _textures[id];
    record.ptr   = RHI::TexturePtr( ptr );
    record.name  = name;
    record.alive = true;
    record.generation++;

    if ( !name.empty() )
        _texNameToHandle[name] = { id };

    return TextureHandle { id };
}

BufferHandle GPUResourcePool::registerExternalBuffer( RHI::IBuffer* ptr, const std::string& name ) {
    return BufferHandle();
}

void GPUResourcePool::clear() {
    std::scoped_lock lock( _mutex );
    _buffers.clear();
    _buffNameToHandle.clear();
    _textures.clear();
    _texNameToHandle.clear();
}

} // namespace Graphics
AXION_NAMESPACE_END