#include "GPUResourcePool.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

GPUResourcePool::GPUResourcePool()
    : RendererSubsystem() {
}

GPUResourcePool::~GPUResourcePool() {
    clear();
    AXION_LOG_INFO( Logger::Module::GFX, "GPU Resource Pool Destroyed" );
}
void GPUResourcePool::initialize( const SubsystemInitContext& ctx ) {
    RendererSubsystem::initialize( ctx );
    AXION_LOG_INFO( Logger::Module::GFX, "GPU Resource Pool Initialized" );
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

SamplerHandle GPUResourcePool::createSampler( const RHI::SamplerDesc& desc, bool allowLookup ) {
    std::scoped_lock lock( _mutex );

    if ( allowLookup && !desc.debugName.empty() && _samplerNameToHandle.count( desc.debugName ) )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Sampler name collision [{}]. Returning existing handle.", desc.debugName );
        return _samplerNameToHandle[desc.debugName];
    }

    auto samplerPtr = _device->createSampler( desc );
    if ( !samplerPtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to create Sampler [{}]", desc.debugName );
        return {};
    }

    uint id = UINT32_MAX;

    // Optimización: Podrías tener una std::queue<uint> _freeIndices para evitar este bucle.
    // Para < 1000 buffers, este bucle es despreciable.
    for ( size_t i = 0; i < _samplers.size(); ++i )
    {
        if ( !_samplers[i].alive )
        {
            id = (uint)i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_samplers.size();
        _samplers.emplace_back();
    }

    auto& record = _samplers[id];
    record.ptr   = std::move( samplerPtr );
    record.name  = desc.debugName;
    record.alive = true;
    record.generation++;

    if ( allowLookup && !desc.debugName.empty() )
        _samplerNameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Sampler ID: {} [{}]", id, desc.debugName );

    return SamplerHandle { id };
}

AccelHandle GPUResourcePool::createAccel( const RHI::AccelDesc& desc, bool instantBuild, bool allowLookup ) {
    std::scoped_lock lock( _mutex );

    if ( allowLookup && !desc.debugName.empty() && _accelNameToHandle.count( desc.debugName ) )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Acceleration Structure name collision [{}]. Returning existing handle.", desc.debugName );
        return _accelNameToHandle[desc.debugName];
    }

    auto accelPtr = _device->createAccel( desc, instantBuild );
    if ( !accelPtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to create Acceleration Structure [{}]", desc.debugName );
        return {};
    }

    uint id = UINT32_MAX;

    // Optimización: Podrías tener una std::queue<uint> _freeIndices para evitar este bucle.
    // Para < 1000 buffers, este bucle es despreciable.
    for ( size_t i = 0; i < _accels.size(); ++i )
    {
        if ( !_accels[i].alive )
        {
            id = (uint)i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_accels.size();
        _accels.emplace_back();
    }

    auto& record = _accels[id];
    record.ptr   = std::move( accelPtr );
    record.name  = desc.debugName;
    record.alive = true;
    record.generation++;

    if ( allowLookup && !desc.debugName.empty() )
        _accelNameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Acceleration Structrue ID: {} [{}]", id, desc.debugName );

    return AccelHandle { id };
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

RHI::ISampler* GPUResourcePool::getSampler( SamplerHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _samplers.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid SamplerHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _samplers[handle.id];
    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead SamplerHandle" );
        return nullptr;
    }

    return record.ptr.get();
}

std::optional<SamplerHandle> GPUResourcePool::findSampler( const std::string& name ) const {
    std::scoped_lock lock( _mutex );
    auto             it = _samplerNameToHandle.find( name );
    if ( it == _samplerNameToHandle.end() )
        return std::nullopt;
    return it->second;
}

void GPUResourcePool::destroySampler( SamplerHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _samplers.size() )
        return;

    auto& record = _samplers[handle.id];
    if ( !record.alive )
        return;

    if ( !record.name.empty() )
        _samplerNameToHandle.erase( record.name );

    record.ptr = nullptr;

    record.alive = false;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Sampler [{}]", record.name );
    record.name.clear();
}

RHI::IAccel* GPUResourcePool::getAccel( AccelHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _accels.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid AccelHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _accels[handle.id];
    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead AccelHandle" );
        return nullptr;
    }

    return record.ptr.get();
}

std::optional<AccelHandle> GPUResourcePool::findAccel( const std::string& name ) const {
    std::scoped_lock lock( _mutex );
    auto             it = _accelNameToHandle.find( name );
    if ( it == _accelNameToHandle.end() )
        return std::nullopt;
    return it->second;
}

void GPUResourcePool::destroyAccel( AccelHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _accels.size() )
        return;

    auto& record = _accels[handle.id];
    if ( !record.alive )
        return;

    if ( !record.name.empty() )
        _accelNameToHandle.erase( record.name );

    record.ptr = nullptr;

    record.alive = false;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Acceleration Structure [{}]", record.name );
    record.name.clear();
}

TextureHandle GPUResourcePool::registerExternalTexture( RHI::TextureOwnerPtr&& ptr, const std::string& name ) {
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
    record.ptr   = std::move( ptr );
    record.name  = name;
    record.alive = true;
    record.generation++;

    if ( !name.empty() )
        _texNameToHandle[name] = { id };

    return TextureHandle { id };
}

BufferHandle GPUResourcePool::registerExternalBuffer( RHI::BufferOwnerPtr&& /*ptr*/, const std::string& /*name*/ ) {
    return BufferHandle();
}

SamplerHandle GPUResourcePool::registerExternalSampler( RHI::SamplerOwnerPtr&& /*ptr*/, const std::string& /*name*/ ) {
    return SamplerHandle();
}

AccelHandle GPUResourcePool::registerExternalAccel( RHI::AccelOwnerPtr&& /*ptr*/, const std::string& /*name*/ ) {
    return AccelHandle();
}

void GPUResourcePool::clear() {
    std::scoped_lock lock( _mutex );
    _buffers.clear();
    _buffNameToHandle.clear();
    _textures.clear();
    _texNameToHandle.clear();
    _samplers.clear();
    _samplerNameToHandle.clear();
    _accels.clear();
    _accelNameToHandle.clear();
}

} // namespace Graphics
AXION_NAMESPACE_END