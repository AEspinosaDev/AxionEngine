#pragma once
#include "Axion/Common/Helpers.h"
#include "RendererSubsystem.h"
#include <Axion/Graphics/Subsystems/IRenderGraph.h>

AXION_NAMESPACE_BEGIN
namespace Graphics {

class RenderGraph final : public IRenderGraph, public RendererSubsystem
{
public:
    RenderGraph();
    ~RenderGraph() override;

    void initialize( const SubsystemInitContext& ctx, const RenderGraphDesc& desc );

    const Description& getDescription() const override { return _desc; }

    void reset() override;
    void execute( RenderGraphSetupFunc setup, RHI::ICommandList* cmd ) override;

    RHI::IBuffer*              getPhysicalBuffer( RGResourceHandle handle ) const override;
    RHI::ITexture*             getPhysicalTexture( RGResourceHandle handle ) const override;
    RHI::IDescriptorAllocator* getDescriptorAllocator( u32 frameIndex ) override;

    void setGarbageCollectionTTL( u32 frames ) override;
    void setAutoSync( bool enable ) override;

private:
    // Bridge methods
    RGResourceHandle createTexture( StringView name, const RHI::TextureDesc& desc ) override;
    RGResourceHandle createBuffer( StringView name, const RHI::BufferDesc& desc ) override;
    RGResourceHandle importTexture( StringView name, TextureHandle handle ) override;
    RGResourceHandle importBuffer( StringView name, BufferHandle handle ) override;

    void  registerPass( StringView name, std::function<void( RenderPassContext& )> executor ) override;
    void  registerDependency( u32 passIndex, RGResourceHandle resource, RHI::ResourceState requiredState, bool isWrite ) override;
    void  storePassData( void* dataPtr, std::function<void()> destructor ) override;
    void* allocateFrameMemory( size_t size, size_t alignment ) override;

    u32 getCurrentPassIndex() const override;

    void runGC();

    void compile();

    IGPUResourcePool*                                _pool      = nullptr;
    IPipelineRegistry*                               _pipelines = nullptr;
    SmallVector<RHI::DescriptorAllocatorOwnerPtr, 3> _descriptorAllocators;
    SmallVector<RHI::SBTAllocatorOwnerPtr, 3>        _sbtAllocators;
    SmallVector<RHI::TransientDataAllocator, 3>      _transientAllocators;

    RenderGraphDesc _desc;

    //--------------------------
    // TRANSIENT DATA
    //--------------------------

    struct RGResource {
        String64                                        name;
        std::variant<RHI::BufferDesc, RHI::TextureDesc> desc;
        std::variant<BufferHandle, TextureHandle>       physicalHandle;
        bool                                            isImported    = false;
        bool                                            isBuffer      = false;
        RHI::ResourceState                              internalState = RHI::ResourceState::Undefined;
    };

    struct RGUsage {
        RGResourceHandle   handle;
        RHI::ResourceState requiredState;
    };

    struct RGBarrier {
        RGResourceHandle   handle;
        RHI::ResourceState before;
        RHI::ResourceState after;
    };

    struct RGPass {
        String64                                  name;
        std::function<void( RenderPassContext& )> executor;
        STLW::Vector<RGUsage>                     reads;
        STLW::Vector<RGUsage>                     writes;

        STLW::Vector<RGBarrier> barriers;
    };

    STLW::Vector<RGResource>            _resources;
    STLW::Vector<RGPass>                _passes;
    STLW::Vector<std::function<void()>> _passDataCleanup;

    STLW::Vector<byte> _frameMemory;
    size_t             _frameOffset = 0;

    //--------------------------
    // CACHED DATA
    //--------------------------

    struct TextureDescHash {
        std::size_t operator()( const RHI::TextureDesc& d ) const {
            std::size_t h = 0;
            Helpers::hashCombine( h, std::hash<u32> {}( d.size.width ) );
            Helpers::hashCombine( h, std::hash<u32> {}( d.size.height ) );
            Helpers::hashCombine( h, std::hash<u32> {}( d.size.depth ) );
            Helpers::hashCombine( h, std::hash<int> {}( (int)d.format ) );
            Helpers::hashCombine( h, std::hash<int> {}( (int)d.viewFlags ) );
            return h;
        }
    };

    struct BufferDescHash {
        std::size_t operator()( const RHI::BufferDesc& d ) const {
            std::size_t h = 0;
            Helpers::hashCombine( h, std::hash<size_t> {}( d.size ) );
            Helpers::hashCombine( h, std::hash<int> {}( (int)d.usageFlags ) );
            Helpers::hashCombine( h, std::hash<int> {}( (int)d.viewFlags ) );
            return h;
        }
    };

    struct CachedBuffer {
        BufferHandle handle;
        u32          lastUsedFrame;
    };

    struct CachedTexture {
        TextureHandle handle;
        u32           lastUsedFrame;
    };

    STLW::UnorderedMultimap<RHI::TextureDesc, CachedTexture, TextureDescHash> _textureCache;
    STLW::UnorderedMultimap<RHI::BufferDesc, CachedBuffer, BufferDescHash>    _bufferCache;

    u32 _frameCounter = 0;
};

} // namespace Graphics
AXION_NAMESPACE_END