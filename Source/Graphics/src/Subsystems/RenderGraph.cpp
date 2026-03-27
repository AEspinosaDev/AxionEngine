#include "RenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Graphics {

RHI::IBuffer* RenderPassContext::getBuffer( RGResourceHandle handle ) const {
    return graph.getPhysicalBuffer( handle );
}

RHI::ITexture* RenderPassContext::getTexture( RGResourceHandle handle ) const {
    return graph.getPhysicalTexture( handle );
}

RHI::IDescriptorSet* RenderPassContext::allocateSet( RHI::IPipelineLayout* layout, uint setIndex ) const {
    return descriptors->allocate( layout, setIndex );
}

RHI::SBT::View RenderPassContext::allocateSBT( const RHI::SBT& sbt, RHI::IRayTracingPipeline* pip ) const {
    return sbtAllocator->allocate( sbt, pip );
}
RGResourceHandle RenderPassBuilder::read( RGResourceHandle resource, RHI::ResourceState requiredState ) {
    if ( resource != RG_INVALID_HANDLE )
    {
        _graph.registerDependency( _passIndex, resource, requiredState, false );
    }
    return resource;
}
RGResourceHandle RenderPassBuilder::write( RGResourceHandle resource, RHI::ResourceState requiredState ) {
    if ( resource != RG_INVALID_HANDLE )
    {
        _graph.registerDependency( _passIndex, resource, requiredState, true );
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

RenderGraph::RenderGraph()
    : IRenderGraph()
    , RendererSubsystem() {}

void RenderGraph::initialize( const SubsystemInitContext& ctx, const RenderGraphDesc& desc ) {
    RendererSubsystem::initialize( ctx );
    _pool      = ctx.pool;
    _pipelines = ctx.pipelines;

    _frameMemory.resize( desc.passDataAllocSize );

    for ( uint i = 0; i < desc.framesInFlight; ++i )
    {

        // Create Descriptor Heap
        RHI::DescriptorAllocatorDesc allocDesc;
        allocDesc.numDescriptors = desc.desciptorSetAllocSize;
        allocDesc.numSamplers    = desc.descriptorMaxSamplers;
        allocDesc.numViews       = desc.descriptorMaxViews;
        allocDesc.debugName      = "RG_Desc_Allocator_Frame_" + std::to_string( i );
        _descriptorAllocators.push_back( _device->createDescriptorAllocator( allocDesc ) );

        // Create Transient Heap
        RHI::TransientAllocatorDesc transDesc;
        transDesc.scratchSize = desc.transientAllocSize;
        transDesc.uploadSize  = desc.transientAllocSize;
        transDesc.debugName   = "RG_Transient_Allocator_Frame_" + std::to_string( i );
        _transientAllocators.push_back( _device->createTransientAllocator( transDesc ) );

        if ( desc.sbtAllocSize > 0 )
        {
            // Create SBT Heap
            RHI::SBTAllocatorDesc sbtAllocDesc;
            sbtAllocDesc.sizeInBytes = static_cast<uint>( desc.sbtAllocSize );
            sbtAllocDesc.debugName   = "RG_SBT_Allocator_Frame_" + std::to_string( i );

            _sbtAllocators.push_back( _device->createSBTAllocator( sbtAllocDesc ) );
        }
    }
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

    auto* currentAllocator = _descriptorAllocators[cmd->getCurrentFrame()].get();
    currentAllocator->reset();
    auto* currentSBTAllocator = _sbtAllocators[cmd->getCurrentFrame()].get();
    currentSBTAllocator->reset();
    auto* currentTransAllocatopr = _transientAllocators[cmd->getCurrentFrame()].get();
    currentTransAllocatopr->reset();

    reset();
    RenderGraphBuilder builder( *this );
    setup( builder );

    if ( _passes.empty() )
    {
        AXION_LOG_WARN_ONCE( Logger::Module::GFX, "RenderGraph has no Passes registered. Skipping execution." );
        return;
    }

    compile();

    // Execute
    RenderPassContext ctx { cmd,
                            currentAllocator,
                            currentSBTAllocator,
                            currentTransAllocatopr,
                            *this,
                            *_pipelines,
                            *_pool };

    for ( const auto& pass : _passes )
    {
        // Call barriers
        if ( _desc.autoSync )
            for ( const auto& b : pass.barriers )
            {
                if ( _resources[b.handle].isBuffer )
                {
                    RHI::IBuffer* rawBuff = nullptr;
                    rawBuff               = getPhysicalBuffer( b.handle );
                    ctx.cmd->barrier( rawBuff, b.after );
                } else
                {
                    RHI::ITexture* rawTex = nullptr;
                    rawTex                = getPhysicalTexture( b.handle );
                    ctx.cmd->barrier( rawTex, b.after );
                }
            }

        pass.executor( ctx );
    }
}

void RenderGraph::compile() {
    for ( auto& res : _resources )
    {
        if ( res.isImported )
        {
            if ( res.isBuffer )
                res.internalState = _pool->getBuffer( std::get<BufferHandle>( res.physicalHandle ) )->getCurrentState();
            else
                res.internalState = _pool->getTexture( std::get<TextureHandle>( res.physicalHandle ) )->getCurrentState();
            continue;
        }

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
                res.physicalHandle = _pool->texture( res.name )
                                         .extent( desc.size.width, desc.size.height, desc.size.depth )
                                         .format( desc.format )
                                         .flags( desc.viewFlags )
                                         .transient()
                                         .create();
            }
            res.internalState = _pool->getTexture( std::get<TextureHandle>( res.physicalHandle ) )->getCurrentState();
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
                res.physicalHandle = _pool->buffer( res.name )
                                         .size( desc.size )
                                         .usage( desc.usageFlags )
                                         .view( desc.viewFlags )
                                         .transient()
                                         .create();
            }
            res.internalState = _pool->getBuffer( std::get<BufferHandle>( res.physicalHandle ) )->getCurrentState();
        }
    }
    // Go through passes to find barriers
    if ( _desc.autoSync )
        for ( auto& pass : _passes )
        {
            pass.barriers.clear();

            for ( const auto& usage : pass.reads )
            {
                auto& res = _resources[usage.handle];

                if ( res.internalState != usage.requiredState )
                {
                    pass.barriers.push_back( { usage.handle,
                                               res.internalState,
                                               usage.requiredState } );

                    res.internalState = usage.requiredState;
                }
            }
            for ( const auto& usage : pass.writes )
            {
                auto& res = _resources[usage.handle];

                if ( res.internalState != usage.requiredState )
                {
                    pass.barriers.push_back( { usage.handle,
                                               res.internalState,
                                               usage.requiredState } );
                    res.internalState = usage.requiredState;
                }
            }
        }
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
        if ( _frameCounter - it->second.lastUsedFrame > _desc.resourceTTL )
        {
            _pool->destroyTexture( it->second.handle );
            AXION_LOG_INFO( Logger::Module::GFX, "RenderGraph GC: Texture released due to inactivity" );
            it = _textureCache.erase( it );
        } else
        {
            ++it;
        }
    }
    for ( auto it = _bufferCache.begin(); it != _bufferCache.end(); )
    {
        if ( _frameCounter - it->second.lastUsedFrame > _desc.resourceTTL )
        {
            _pool->destroyBuffer( it->second.handle );
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

void RenderGraph::registerDependency( uint passIndex, RGResourceHandle resource, RHI::ResourceState requiredState, bool isWrite ) {
    if ( passIndex < _passes.size() )
    {
        if ( isWrite )
            _passes[passIndex].writes.push_back( { resource, requiredState } );
        else
            _passes[passIndex].reads.push_back( { resource, requiredState } );
    }
}

RHI::IBuffer* RenderGraph::getPhysicalBuffer( RGResourceHandle handle ) const {
    if ( handle >= _resources.size() )
        return nullptr;
    const auto& res = _resources[handle];
    if ( !res.isBuffer )
        return nullptr; // Error de tipo

    if ( auto* h = std::get_if<BufferHandle>( &res.physicalHandle ) )
        return _pool->getBuffer( *h );
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
        return _pool->getTexture( *h );
    else
        return nullptr;
}

RHI::IDescriptorAllocator* RenderGraph::getDescriptorAllocator( uint frameIndex ) {
    AXION_LOG_ASSERT( frameIndex < _descriptorAllocators.size(), Logger::Module::RHI, "Trying to access null RG DescritporAllocator | Invalid frame number" );
    return _descriptorAllocators[frameIndex].get();
}

void RenderGraph::setGarbageCollectionTTL( uint frames ) {
    _desc.resourceTTL = frames;
}

void RenderGraph::setAutoSync( bool enable ) {
    _desc.autoSync = enable;
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

void RenderGraph::storePassData( void* /*dataPtr*/, std::function<void()> destructor ) {
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