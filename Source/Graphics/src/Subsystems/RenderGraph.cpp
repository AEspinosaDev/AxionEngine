#include "Axion/Graphics/Subsystems/RenderGraph.h"
#include "RenderGraph.hpp"

AXION_NAMESPACE_BEGIN
namespace Graphics {

RHI::IBuffer* RenderPassContext::getBuffer( RGResourceHandle handle ) const {
    return graph.getPhysicalBuffer( handle );
}

RHI::ITexture* RenderPassContext::getTexture( RGResourceHandle handle ) const {
    return graph.getPhysicalTexture( handle );
}

RGResourceHandle RenderPassBuilder::read( RGResourceHandle resource ) {
    if ( resource != RG_INVALID_HANDLE )
    {
        _graph.registerDependency( _passIndex, resource, false );
    }
    return resource;
}
RGResourceHandle RenderPassBuilder::write( RGResourceHandle resource ) {
    if ( resource != RG_INVALID_HANDLE )
    {
        _graph.registerDependency( _passIndex, resource, true );
    }
    return resource;
}
RenderGraphBuilder::TextureBuilder RenderGraphBuilder::texture( const std::string& name ) {
    return TextureBuilder( *this, name );
}

RenderGraphBuilder::BufferBuilder RenderGraphBuilder::buffer( const std::string& name ) {
    return BufferBuilder( *this, name );
}

RGResourceHandle RenderGraphBuilder::create( const std::string& name, const RHI::TextureDesc& desc ) {
    return _graph.createTexture( name, desc );
}

RGResourceHandle RenderGraphBuilder::create( const std::string& name, const RHI::BufferDesc& desc ) {
    return _graph.createBuffer( name, desc );
}

RGResourceHandle RenderGraphBuilder::import( const std::string& name, TextureHandle handle ) {
    return _graph.importTexture( name, handle );
}

RGResourceHandle RenderGraphBuilder::import( const std::string& name, BufferHandle handle ) {
    return _graph.importBuffer( name, handle );
}

// =============================================================================
// RENDER GRAPH IMPLEMENTATION
// =============================================================================

RenderGraph::RenderGraph( IGPUResourcePool& pool, IPipelineRegistry& pipelines, ulong allocSize, uint resourceTTL )
    : _pool( pool )
    , _pipelines( pipelines )
    , _kResourceTTL( resourceTTL ) {
    _frameMemory.resize( allocSize );
    AXION_LOG_INFO( Logger::Module::GFX, "RenderGraph Subsystem Initialized Succesfully" );
}

RenderGraph::~RenderGraph() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying RenderGraph" );
    reset();
}

void RenderGraph::reset() {
    _frameCounter++;

    runGC();

    for ( auto& res : _resources )
    {
        if ( !res.isImported )
        {
            if ( res.isBuffer )
            {
                if ( auto* h = std::get_if<BufferHandle>( &res.physicalHandle ) )
                {
                    const auto& desc = std::get<RHI::BufferDesc>( res.desc );
                    _bufferCache.insert( { desc, { *h, _frameCounter } } );
                }
            } else
            {
                if ( auto* h = std::get_if<TextureHandle>( &res.physicalHandle ) )
                {
                    const auto& desc = std::get<RHI::TextureDesc>( res.desc );
                    _textureCache.insert( { desc, { *h, _frameCounter } } );
                }
            }
        }
    }

    _resources.clear();
    _passes.clear();

    for ( auto& cleanupFunc : _passDataCleanup )
    {
        cleanupFunc();
    }
    _passDataCleanup.clear();
    _frameOffset = 0;
}

void RenderGraph::execute( RenderGraphSetupFunc setup, RHI::ICommandList* cmd ) {

    reset();
    RenderGraphBuilder builder( *this );
    setup( builder );

    if ( _passes.empty() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "RenderGraph has no Passes registered. Skipping execution." );
        return;
    }

    compile();

    // Execute
    RenderPassContext ctx { cmd, *this, _pipelines, _pool };
    for ( const auto& pass : _passes )
    {
        // TODO: Insertar Barreras automáticas aquí antes de ejecutar
        // _insertBarriers( pass, cmd );

        pass.executor( ctx );
    }
}

void RenderGraph::compile() {
    for ( auto& res : _resources )
    {
        if ( res.isImported )
            continue;

        if ( !res.isBuffer )
        {
            const auto& desc = std::get<RHI::TextureDesc>( res.desc );

            auto it = _textureCache.find( desc );

            if ( it != _textureCache.end() )
            {
                res.physicalHandle = it->second.handle;
                _textureCache.erase( it ); // O(1)
            } else
            {
                res.physicalHandle = _pool.texture( res.name )
                                         .extent( desc.size.width, desc.size.height, desc.size.depth )
                                         .format( desc.format )
                                         .flags( desc.viewFlags )
                                         .create();
            }
        } else
        {
            const auto& desc = std::get<RHI::BufferDesc>( res.desc );

            auto it = _bufferCache.find( desc );

            if ( it != _bufferCache.end() )
            {
                res.physicalHandle = it->second.handle;
                _bufferCache.erase( it );
            } else
            {
                res.physicalHandle = _pool.buffer( res.name )
                                         .size( desc.size )
                                         .usage( desc.usageFlags )
                                         .view( desc.viewFlags )
                                         .create();
            }
        }
    }
    // 2. (Futuro) Calcular Barreras
    // Iterar pasadas, ver qué leen/escriben, comparar con estado actual del recurso,
    // e insertar transiciones.}
}

// --- Helpers Internos ---

uint RenderGraph::getCurrentPassIndex() const {
    return (uint)_passes.size(); // La próxima a insertar
}

void RenderGraph::runGC() {
    if ( _frameCounter % 60 != 0 )
        return;

    for ( auto it = _textureCache.begin(); it != _textureCache.end(); )
    {
        if ( _frameCounter - it->second.lastUsedFrame > _kResourceTTL )
        {
            _pool.destroyTexture( it->second.handle );
            AXION_LOG_INFO( Logger::Module::GFX, "RenderGraph GC: Texture released due to inactivity" );
            it = _textureCache.erase( it );
        } else
        {
            ++it;
        }
    }
    for ( auto it = _bufferCache.begin(); it != _bufferCache.end(); )
    {
        if ( _frameCounter - it->second.lastUsedFrame > _kResourceTTL )
        {
            _pool.destroyBuffer( it->second.handle );
            AXION_LOG_INFO( Logger::Module::GFX, "RenderGraph GC: Buffer released due to inactivity" );
            it = _bufferCache.erase( it );
        } else
        {
            ++it;
        }
    }
}
void RenderGraph::registerPass( const std::string& name, std::function<void( RenderPassContext& )> executor ) {
    RGPass pass;
    pass.name     = name;
    pass.executor = std::move( executor );
    _passes.push_back( std::move( pass ) );
}

void RenderGraph::registerDependency( uint passIndex, RGResourceHandle resource, bool isWrite ) {
    if ( passIndex < _passes.size() )
    {
        if ( isWrite )
            _passes[passIndex].writes.push_back( resource );
        else
            _passes[passIndex].reads.push_back( resource );
    }
}

RHI::IBuffer* RenderGraph::getPhysicalBuffer( RGResourceHandle handle ) const {
    if ( handle >= _resources.size() )
        return nullptr;
    const auto& res = _resources[handle];
    if ( !res.isBuffer )
        return nullptr; // Error de tipo

    if ( auto* h = std::get_if<BufferHandle>( &res.physicalHandle ) )
        return _pool.getBuffer( *h );
    else
        return nullptr;
}

RHI::ITexture* RenderGraph::getPhysicalTexture( RGResourceHandle handle ) const {
    if ( handle >= _resources.size() )
        return nullptr;
    const auto& res = _resources[handle];
    if ( res.isBuffer )
        return nullptr;

    if ( auto* h = std::get_if<TextureHandle>( &res.physicalHandle ) )
        return _pool.getTexture( *h );
    else
        return nullptr;
}

void RenderGraph::setGarbageCollectionTTL( uint frames ) {
    _kResourceTTL = frames;
}

RGResourceHandle RenderGraph::createTexture( const std::string& name, const RHI::TextureDesc& desc ) {
    RenderGraph::RGResource res;
    res.name           = name;
    res.desc           = desc;
    res.isImported     = false;
    res.isBuffer       = false;
    res.physicalHandle = TextureHandle {}; // Empty until compile

    _resources.push_back( res );
    return (RGResourceHandle)( _resources.size() - 1 );
}
RGResourceHandle RenderGraph::createBuffer( const std::string& name, const RHI::BufferDesc& desc ) {
    RenderGraph::RGResource res;
    res.name           = name;
    res.desc           = desc;
    res.isImported     = false;
    res.isBuffer       = true;
    res.physicalHandle = BufferHandle {}; // Empty until compile

    _resources.push_back( res );
    return (RGResourceHandle)( _resources.size() - 1 );
}
RGResourceHandle RenderGraph::importTexture( const std::string& name, TextureHandle handle ) {
    RenderGraph::RGResource res;
    res.name           = name;
    res.isImported     = true;
    res.isBuffer       = false;
    res.physicalHandle = handle;

    _resources.push_back( res );
    return (RGResourceHandle)( _resources.size() - 1 );
}
RGResourceHandle RenderGraph::importBuffer( const std::string& name, BufferHandle handle ) {
    RenderGraph::RGResource res;
    res.name           = name;
    res.isImported     = true;
    res.isBuffer       = true;
    res.physicalHandle = handle;

    _resources.push_back( res );
    return (RGResourceHandle)( _resources.size() - 1 );
}

void RenderGraph::storePassData( void* dataPtr, std::function<void()> destructor ) {
    _passDataCleanup.push_back( destructor );
}

void* RenderGraph::allocateFrameMemory( size_t size, size_t alignment ) {
    size_t currentAddress = (size_t)_frameMemory.data() + _frameOffset;
    size_t adjustment     = 0;

    size_t mask = alignment - 1;
    if ( currentAddress & mask )
    {
        adjustment = alignment - ( currentAddress & mask );
    }

    if ( _frameOffset + adjustment + size > _frameMemory.size() )
    {
        // Opción A: Crecer el vector (invalida punteros anteriores -> PELIGROSO si no se tiene cuidado)
        // Opción B: Tener un vector de vectores (Chunked Linear Allocator).
        // Opción C (Simple): Resize y avisar (ok porque esto ocurre en Setup, antes de ejecutar)
        _frameMemory.resize( _frameMemory.size() * 2 );
        // Recalcular dirección base por si el resize movió la memoria
        currentAddress = (size_t)_frameMemory.data() + _frameOffset;
    }

    size_t alignedOffset = _frameOffset + adjustment;
    void*  ptr           = &_frameMemory[alignedOffset];

    _frameOffset = alignedOffset + size;

    return ptr;
}

} // namespace Graphics
AXION_NAMESPACE_END