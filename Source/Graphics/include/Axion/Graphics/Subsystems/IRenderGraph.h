#pragma once
#include "Axion/Graphics/RHI/ICommandList.h"
#include "Axion/Graphics/RHI/TransientDataAllocator.h"
#include "Axion/Graphics/ResourceBuilders.h"
#include "Axion/Graphics/Subsystems/IGPUResourcePool.h"
#include "Axion/Graphics/Subsystems/IPipelineRegistry.h"
#include <functional>
#include <string>
#include <vector>

AXION_NAMESPACE_BEGIN
namespace Graphics {

// Forward Declarations
class IRenderGraph;

/// @brief Logical handle representing a resource within the RenderGraph frame.
using RGResourceHandle                   = u32;
const RGResourceHandle RG_INVALID_HANDLE = UINT32_MAX;

/// @brief Context passed to the execution lambda of a render pass.
/// Provides access to physical resources and command recording.
struct RenderPassContext {
    RHI::ICommandList*           cmd;            ///< Command list for recording GPU commands.
    RHI::IDescriptorAllocator*   descriptors;    ///< Descriptor Allocate to register GPU visible DescriptorSets.
    RHI::ISBTAllocator*          sbtAllocator;   ///< SBT AllocatOR to register GPU visible Shader Groups for RTX.
    RHI::TransientDataAllocator* transAllocator; ///< Transient Resource AllocatOR to upload data.

    const IRenderGraph& graph;     ///< Reference to the graph for handle resolution.
    IPipelineRegistry&  pipelines; ///< Access to compiled PSOs.
    IGPUResourcePool&   resources; ///< Access to physical GPU resources.

    /// @brief Resolves a logical buffer handle to its physical pointer.
    RHI::IBuffer* getBuffer( RGResourceHandle handle ) const;

    /// @brief Resolves a logical texture handle to its physical pointer.
    RHI::ITexture* getTexture( RGResourceHandle handle ) const;

    RHI::IDescriptorSet* allocateSet( RHI::IPipelineLayout* layout, u32 setIndex ) const;
    RHI::SBT::Allocation allocateSBT( const RHI::SBT& sbt, RHI::IRayTracingPipeline* pip ) const;
    BufferSlice          uploadDynamic( const void* data, u64 size, u64 alignment = 256 ) const;
};

/// @brief Helper class to declare resource usage during the Setup phase.
class RenderPassBuilder
{
public:
    RenderPassBuilder( IRenderGraph& graph, u32 passIndex )
        : _graph( graph )
        , _passIndex( passIndex ) {}

    /// @brief Declares read access to a resource.
    /// @return The handle to use for reading.
    RGResourceHandle read( RGResourceHandle resource, RHI::ResourceState requiredState = RHI::ResourceState::ShaderResource );

    /// @brief Declares write access to a resource.
    /// @return The handle to use for writing (supports renaming in future).
    RGResourceHandle write( RGResourceHandle resource, RHI::ResourceState requiredState = RHI::ResourceState::UnorderedAccess );

private:
    IRenderGraph& _graph;
    u32           _passIndex;
};

/// @brief Main entry point for defining the frame graph structure.
class RenderGraphBuilder
{
public:
    RenderGraphBuilder( IRenderGraph& graph )
        : _graph( graph ) {}

    class TextureBuilder;
    class BufferBuilder;

    /// @brief Starts building a transient texture description.
    TextureBuilder texture( StringView name );

    /// @brief Starts building a transient buffer description.
    BufferBuilder buffer( StringView name );

    /// @brief Imports an existing physical texture into the graph.
    RGResourceHandle import( StringView name, TextureHandle handle );

    /// @brief Imports an existing physical buffer into the graph.
    RGResourceHandle import( StringView name, BufferHandle handle );

    /// @brief Adds a new render pass to the graph.
    /// @tparam PassData Struct type to hold pass-specific data (handles, settings).
    /// @param name Debug name of the pass.
    /// @param setup Lambda for declaring resource dependencies.
    /// @param execute Lambda for recording GPU commands.
    template <typename PassData>
    void addPass(
        StringView                                                 name,
        std::function<void( RenderPassBuilder&, PassData& )>       setup,
        std::function<void( const PassData&, RenderPassContext& )> execute );

    /// @brief Adds a new render pass to the graph.
    /// @tparam PassData Struct type to hold pass-specific data (handles, settings).
    /// @param name Debug name of the pass.
    /// @param seedData Data to copy onto the pass.
    /// @param setup Lambda for declaring resource dependencies.
    /// @param execute Lambda for recording GPU commands.
    template <typename PassData>
    void addPass(
        StringView                                                 name,
        const PassData&                                            seedData,
        std::function<void( RenderPassBuilder&, PassData& )>       setup,
        std::function<void( const PassData&, RenderPassContext& )> execute );

    template <typename PassClass>
    void addPass( StringView name, PassClass& passInstance );

private:
    // Internal proxies calling virtual methods on IRenderGraph
    RGResourceHandle create( StringView name, const RHI::TextureDesc& desc );
    RGResourceHandle create( StringView name, const RHI::BufferDesc& desc );
    IRenderGraph&    _graph;
};

/// @brief Function signature for the user-defined frame setup logic.
using RenderGraphSetupFunc = std::function<void( RenderGraphBuilder& builder )>;

/// @brief Abstract interface for the RenderGraph subsystem.
/// Manages pass execution, resource aliasing, and barrier insertion.
class IRenderGraph
{
public:
    struct Description {
        u32  framesInFlight;
        u64  passDataAllocSize;
        u32  desciptorSetAllocSize;
        u32  descriptorMaxViews    = 256;
        u32  descriptorMaxSamplers = 64;
        u64  sbtAllocSize          = 0;
        u64  transientAllocSize    = 64 * 1024 * 1024;
        u32  resourceTTL;
        bool autoSync = true;
    };

    virtual ~IRenderGraph() = default;

    virtual const Description& getDescription() const = 0;

    /// @brief Resets the graph state and recycles transient resources.
    virtual void reset() = 0;

    /// @brief Compiles and executes the frame graph.
    /// @param setup User lambda defining the passes.
    /// @param cmd Command list to record into.
    virtual void execute( RenderGraphSetupFunc setup, RHI::ICommandList* cmd ) = 0;

    // -- Internal Access (Virtual) --

    virtual RHI::IBuffer*              getPhysicalBuffer( RGResourceHandle handle ) const  = 0;
    virtual RHI::ITexture*             getPhysicalTexture( RGResourceHandle handle ) const = 0;
    virtual RHI::IDescriptorAllocator* getDescriptorAllocator( u32 frameIndex )            = 0;

    /// @brief Sets the Time-To-Live for cached transient resources.
    virtual void setGarbageCollectionTTL( u32 frames ) = 0;

    /// @brief Enables Automatic Barriers.
    virtual void setAutoSync( bool enable ) = 0;

protected:
    // -- Bridge Methods (Implemented by Concrete Class) --

    virtual RGResourceHandle createTexture( StringView name, const RHI::TextureDesc& desc ) = 0;
    virtual RGResourceHandle createBuffer( StringView name, const RHI::BufferDesc& desc )   = 0;
    virtual RGResourceHandle importTexture( StringView name, TextureHandle handle )         = 0;
    virtual RGResourceHandle importBuffer( StringView name, BufferHandle handle )           = 0;

    virtual void  registerPass( StringView name, std::function<void( RenderPassContext& )> executor )                            = 0;
    virtual void  registerDependency( u32 passIndex, RGResourceHandle resource, RHI::ResourceState requiredState, bool isWrite ) = 0;
    virtual void  storePassData( void* dataPtr, std::function<void()> destructor )                                               = 0;
    virtual void* allocateFrameMemory( size_t size, size_t alignment )                                                           = 0;

    virtual u32 getCurrentPassIndex() const = 0;

    friend class RenderGraphBuilder;
    friend class RenderPassBuilder;
};

typedef IRenderGraph::Description RenderGraphDesc;

// -----------------------------------------------------------------------------
// TEMPLATE & BUILDER IMPLEMENTATIONS
// -----------------------------------------------------------------------------

template <typename PassData>
void RenderGraphBuilder::addPass(
    StringView                                                 name,
    std::function<void( RenderPassBuilder&, PassData& )>       setup,
    std::function<void( const PassData&, RenderPassContext& )> execute ) {
    // 1. Allocate Data (Linear Allocator)
    void*     rawMemory = _graph.allocateFrameMemory( sizeof( PassData ), alignof( PassData ) );
    PassData* data      = new ( rawMemory ) PassData();

    // 2. Register Destructor (No deallocation, just cleanup)
    _graph.storePassData( data, [data]() {
        data->~PassData();
    } );

    // 3. Register Execute Phase
    _graph.registerPass( name, [execute, data]( RenderPassContext& ctx ) {
        execute( *data, ctx );
    } );
    // 4. Setup Phase
    RenderPassBuilder builder( _graph, _graph.getCurrentPassIndex() - 1 );
    setup( builder, *data );
}
template <typename PassData>
void RenderGraphBuilder::addPass(
    StringView                                                 name,
    const PassData&                                            seedData,
    std::function<void( RenderPassBuilder&, PassData& )>       setup,
    std::function<void( const PassData&, RenderPassContext& )> execute ) {
    // 1. Allocate Memory
    void*     rawMemory = _graph.allocateFrameMemory( sizeof( PassData ), alignof( PassData ) );
    PassData* data      = new ( rawMemory ) PassData( seedData );

    // 3. Register Destructor
    _graph.storePassData( data, [data]() { data->~PassData(); } );

    // 4. Register Execute
    _graph.registerPass( name, [execute, data]( RenderPassContext& ctx ) {
        execute( *data, ctx );
    } );

    // 5. Setup Phase
    RenderPassBuilder builder( _graph, _graph.getCurrentPassIndex() - 1 );
    setup( builder, *data );
}

template <typename PassClass>
void RenderGraphBuilder::addPass( StringView name, PassClass& passInstance ) {
    using PassData = typename PassClass::Data;
    addPass<PassData>( name, [&]( RenderPassBuilder& pb, PassData& data ) { passInstance.setup( pb, data ); }, [&]( const PassData& data, RenderPassContext& ctx ) { passInstance.execute( data, ctx ); } );
}

class RenderGraphBuilder::TextureBuilder : public TextureBuilderBase<TextureBuilder>
{
public:
    TextureBuilder( RenderGraphBuilder& builder, StringView name )
        : TextureBuilderBase( name )
        , _builder( builder ) {}

    RGResourceHandle create() {
        return _builder.create( _desc.debugName, _desc );
    }

private:
    RenderGraphBuilder& _builder;
};

class RenderGraphBuilder::BufferBuilder : public BufferBuilderBase<BufferBuilder>
{
public:
    BufferBuilder( RenderGraphBuilder& builder, StringView name )
        : BufferBuilderBase( name )
        , _builder( builder ) {}

    RGResourceHandle create() {
        return _builder.create( _desc.debugName, _desc );
    }

private:
    RenderGraphBuilder& _builder;
};

} // namespace Graphics
AXION_NAMESPACE_END