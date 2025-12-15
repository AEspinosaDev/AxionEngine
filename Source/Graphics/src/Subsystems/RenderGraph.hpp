#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/Device.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Graphics {

DEFINE_UNIQUE_PTR_FOR_TYPE( RenderGraph, RenderGraph )

class RenderGraph final : public IRenderGraph
{
public:
    RenderGraph( RHI::IDevice*          device,
                 IGPUResourcePool&      pool,
                 IPipelineRegistry&     pipelines,
                 const RenderGraphDesc& desc );
    ~RenderGraph() override;

    const Description& getDescription() const override { return _desc; }

    void reset() override;
    void execute( RenderGraphSetupFunc setup, RHI::ICommandList* cmd ) override;

    RHI::IBuffer*  getPhysicalBuffer( RGResourceHandle handle ) const override;
    RHI::ITexture* getPhysicalTexture( RGResourceHandle handle ) const override;

    void setGarbageCollectionTTL( uint frames ) override;
    void setAutoSync( bool enable ) override;

private:
    // Bridge methods
    RGResourceHandle createTexture( const std::string& name, const RHI::TextureDesc& desc ) override;
    RGResourceHandle createBuffer( const std::string& name, const RHI::BufferDesc& desc ) override;
    RGResourceHandle importTexture( const std::string& name, TextureHandle handle ) override;
    RGResourceHandle importBuffer( const std::string& name, BufferHandle handle ) override;

    void  registerPass( const std::string& name, std::function<void( RenderPassContext& )> executor ) override;
    void  registerDependency( uint passIndex, RGResourceHandle resource, RHI::ResourceState requiredState, bool isWrite ) override;
    void  storePassData( void* dataPtr, std::function<void()> destructor ) override;
    void* allocateFrameMemory( size_t size, size_t alignment ) override;

    uint getCurrentPassIndex() const override;

    void runGC();

    void compile();

    IGPUResourcePool&                        _pool;
    IPipelineRegistry&                       _pipelines;
    std::vector<RHI::DescriptorAllocatorPtr> _descriptorAllocators;
    std::vector<RHI::SBTAllocatorPtr>        _sbtAllocators;

    RenderGraphDesc _desc;

    //--------------------------
    // TRANSIENT DATA
    //--------------------------

    struct RGResource {
        std::string                                     name;
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
        std::string                               name;
        std::function<void( RenderPassContext& )> executor;
        std::vector<RGUsage>                      reads;
        std::vector<RGUsage>                      writes;

        std::vector<RGBarrier> barriers;
    };

    std::vector<RGResource>            _resources;
    std::vector<RGPass>                _passes;
    std::vector<std::function<void()>> _passDataCleanup;

    std::vector<uchar> _frameMemory;
    size_t             _frameOffset = 0;

    //--------------------------
    // CACHED DATA
    //--------------------------

    struct TextureDescHash {
        std::size_t operator()( const RHI::TextureDesc& d ) const {
            std::size_t h = 0;
            Helpers::hashCombine( h, std::hash<uint32_t> {}( d.size.width ) );
            Helpers::hashCombine( h, std::hash<uint32_t> {}( d.size.height ) );
            Helpers::hashCombine( h, std::hash<uint32_t> {}( d.size.depth ) );
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
        uint32_t     lastUsedFrame;
    };

    struct CachedTexture {
        TextureHandle handle;
        uint32_t      lastUsedFrame;
    };

    std::unordered_multimap<RHI::TextureDesc, CachedTexture, TextureDescHash> _textureCache;
    std::unordered_multimap<RHI::BufferDesc, CachedBuffer, BufferDescHash>    _bufferCache;

    uint _frameCounter = 0;
};

} // namespace Graphics
AXION_NAMESPACE_END