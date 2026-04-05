#include "Renderer.h"
#include "Axion/Graphics/Platforms/IWin32.h"
#include "Axion/Graphics/RHI/DX12/IDX12Device.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererOwnerPtr Graphics::createRenderer( IWindow* wnd, const RendererSettings& settings ) {
    auto rnd = Memory::makeOwned<Renderer>( wnd, settings );
    AXION_LOG_INFO( Logger::Module::GFX, "Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, "{}", rnd->toString() );
    return rnd;
}

Renderer::Renderer( IWindow* wnd, const RendererSettings& settings )
    : _wnd( wnd )
    , _setts( settings )
    , _FRAMES_IN_FLIGHT( static_cast<u32>( settings.bufferingType ) + 1 ) {

    AXION_LOG_ASSERT( _wnd, Logger::Module::GFX, "Window is NULL | Renderer needs Window. If no window needed, use Headless Renderer" );
    _frameFences.resize( _FRAMES_IN_FLIGHT );

    AXION_LOG_ASSERT( _setts.RGmaxAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.host.maxPersistentAlloc,
                      Logger::Module::GFX,
                      "RenderGraph persistent allocation exceeds Host memory budget." );
    AXION_LOG_ASSERT( _setts.RGmaxSBTAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.device.maxUploadAlloc,
                      Logger::Module::GFX,
                      "SBT allocation exceeds Device Upload budget." );
    AXION_LOG_ASSERT( _setts.RGmaxSBTAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.device.maxUploadAlloc,
                      Logger::Module::GFX,
                      "Transient allocation exceeds Device Upload budget." );

    // -------------------------------
    // Per Graphics API Device Creation
    // -------------------------------
    switch ( _setts.gfxApi )
    {
        case API::DirectX12:
            RHI::DX12DeviceDesc desc {
                .preferredDeviceID    = _setts.selectedDeviceID,
                .enableDebugLayer     = _setts.debugMode,
                .maxTextureAlloc      = _setts.memory.device.maxTextureAlloc,
                .maxBufferAlloc       = _setts.memory.device.maxBufferAlloc,
                .maxRenderTargetAlloc = _setts.memory.device.maxRenderTargetAlloc,
                .maxUploadAlloc       = _setts.memory.device.maxUploadAlloc,
                .strictMemoryCap      = true
            };
            _device = RHI::createDX12Device( desc );
            break;
            // case GraphicsAPI::Vulkan:

            //     break;

            // default:
            //     break;
    }

    _swapchain      = _device->createSwapchain( wnd->getNativeObject(), { .size = wnd->getSettings().size, .imageCount = _FRAMES_IN_FLIGHT, .presentMode = settings.presentMode } );
    _resizeCbHandle = _wnd->onResize().subscribe( [this]( const Event::WindowResizeEvent& e ) { this->windowCallback( { e.width, e.height } ); } );
    // Init Command List
    _commandList = _device->createCommandList( { .queueType = RHI::QueueType::Graphics, .numFrames = _FRAMES_IN_FLIGHT, .debugName = "Graphics Command List" } );

    // -------------------------------
    // SUBSYSTEMS Initialization
    // -------------------------------
    SubsystemInitContext ctx;
    ctx.device    = _device.get();
    ctx.pool      = &_resourcePool;
    ctx.shaderReg = &_shaderRegistry;
    ctx.pipelines = &_pipelineRegistry;

    _resourcePool.initialize( ctx );
    _shaderRegistry.initialize( ctx );
    _pipelineRegistry.initialize( ctx );

    RenderGraphDesc RGDesc = {
        .framesInFlight        = _FRAMES_IN_FLIGHT,
        .passDataAllocSize     = _setts.RGmaxAlloc,
        .desciptorSetAllocSize = _setts.RGmaxDescriptorsPerFrame,
        .descriptorMaxViews    = _setts.RGmaxViewsPerFrame,
        .descriptorMaxSamplers = _setts.RGmaxSamplersPerFrame,
        .sbtAllocSize          = _setts.RGmaxSBTAlloc,
        .transientAllocSize    = _setts.RGmaxTransientAlloc,
        .resourceTTL           = (u32)_setts.GCMode,
        .autoSync              = _setts.autoSync };
    _renderGraph.initialize( ctx, RGDesc );

    generateSwapchainHandles();

    // Init GUI Backend
    if ( _setts.enableGui )
    {
        RHI::GUIBackendDesc guiDesc = {
            .platform           = _wnd->getPlatformType(),
            .backbufferFormat   = _setts.backbufferFormat,
            .framesInFlight     = _FRAMES_IN_FLIGHT,
            .nativeWindowHandle = _wnd->getNativeObject(),
        };
        switch ( _setts.gfxApi )
        {
            case API::DirectX12:
                _guiBackend = RHI::createGUIBackendForDX12( _device.get(), guiDesc );
                break;
                // case GraphicsAPI::Vulkan:
                //     break;
        }
        AXION_LOG_ASSERT( _guiBackend, Logger::Module::Core, "GUI Backend is not initialized in Renderer" );
    }
}

Renderer::~Renderer() {
    destroy();
}

void Renderer::render( RenderGraphSetupFunc setup ) {

    if ( _wnd->minimized() )
        return;

    if ( _pendingResize )
    {
        _device->waitIdle();

        for ( auto& handle : _swapchainHandles )
        {
            if ( handle.isValid() )
                _resourcePool.destroyTexture( handle );
        }

        auto desc = _swapchain->getDescription();
        desc.size = _wnd->getSettings().size;
        _swapchain->update( desc );

        generateSwapchainHandles();

        _currentFrame = _swapchain->acquireNextImage();

        _pendingResize = false;
    }

    _commandList->setCurrentFrame( _currentFrame );
    _commandList->begin();

    _renderGraph.execute( setup, _commandList.get() );

    _commandList->end();

    _device->executeCommandLists(
        { _commandList.get() },
        RHI::QueueType::Graphics,
        _frameFences[_currentFrame] );

    _swapchain->present();

    _currentFrame = _swapchain->acquireNextImage();

    _device->waitForFrame( _frameFences[_currentFrame], RHI::QueueType::Graphics );

    _frameNumber++;
}

void Renderer::destroy() {
    _device->queueWaitIdle( RHI::QueueType::Graphics, _frameFences[_currentFrame] );

    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Renderer" );
}
bool Renderer::isHeadless() {
    return false;
}

IWindow* Renderer::getWindow() {
    return _wnd;
}

const RHI::DeviceOwnerPtr& Renderer::getDevice() const {
    return _device;
}

RHI::IDescriptorAllocator* Renderer::getFrameDescriptorAllocator( u32 frameIndex ) {
    return _renderGraph.getDescriptorAllocator( frameIndex );
}

const RHI::IGUIBackend* Renderer::getGUIBackend() const {
    return _guiBackend.get();
}

TextureHandle Renderer::getCurrentBackbufferHandle() const {
    return _swapchainHandles[_currentFrame];
}

u32 Renderer::getCurrentFrameIndex() const {
    return _currentFrame;
}

STLW::String Renderer::toString() const {
    // return fmt::format(
    //     "Settings:\n"
    //     "  Graphics API: {}\n"
    //     "  Buffering Type: {}\n"
    //     "  Debug Mode: {}\n"
    //     "  Present Mode: {}\n"
    //     "  Output Format: {}",
    //     gfxApiToString( gfxApi ),
    //     bufferingTypeToString( bufferingType ),
    //     debugMode,
    //     presentModeToString( presentMode ),
    //     formatToString( backbufferFormat ) );
    const u64 totalVRAM = _setts.memory.device.maxTextureAlloc +
                          _setts.memory.device.maxBufferAlloc +
                          _setts.memory.device.maxRenderTargetAlloc;

    const u64 totalUpload = _setts.memory.device.maxUploadAlloc;

    const u64 totalHostRAM = _setts.memory.host.maxPersistentAlloc +
                             ( _setts.memory.host.maxTransientAllocPerFrame * _FRAMES_IN_FLIGHT );

    return static_cast<STLW::String>( fmt::format(
        "Renderer Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n"
        "  --- Memory Budgets ---\n"
        "  Dedicated VRAM (Textures, Buffers, RTs): {} MB\n"
        "  Mapped Upload Memory (PCIe): {} MB\n"
        "  Host RAM (Persistent + Transient x {}): {} MB\n",
        (u32)_setts.bufferingType + 1,
        _setts.debugMode,
        totalVRAM / ( 1024 * 1024 ),
        totalUpload / ( 1024 * 1024 ),
        _FRAMES_IN_FLIGHT,
        totalHostRAM / ( 1024 * 1024 ) ) );
}

bool Renderer::instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands ) {
    _device->oneTimeSubmit( commands );
    return true;
}

void Renderer::setWindow( IWindow* wnd ) {
    _wnd            = wnd;
    _swapchain      = _device->createSwapchain( wnd->getNativeObject(), { .size = wnd->getSettings().size, .imageCount = _FRAMES_IN_FLIGHT, .presentMode = _setts.presentMode } );
    _resizeCbHandle = _wnd->onResize().subscribe( [this]( const Event::WindowResizeEvent& e ) { this->windowCallback( { e.width, e.height } ); } );
}

const RendererSettings& Renderer::getSettings() const {
    return _setts;
}

IGPUResourcePool& Renderer::resources() {
    return _resourcePool;
}

IShaderRegistry& Renderer::shaders() {
    return _shaderRegistry;
}

IPipelineRegistry& Renderer::pipelines() {
    return _pipelineRegistry;
}

void Renderer::windowCallback( const Extent2D& newSize ) {
    if ( newSize.width > 0 || newSize.height > 0 )
    {
        _pendingResize = true;
    }
}
void Renderer::generateSwapchainHandles() {
    _swapchainHandles.clear();
    auto images = _swapchain->releaseImages();

    for ( size_t i = 0; i < images.size(); ++i )
    {
        auto handle = _resourcePool.registerExternalTexture( std::move( images[i] ), "Backbuffer_" + std::to_string( i ) );
        _swapchainHandles.pushBack( handle );
    }
}

} // namespace Graphics

AXION_NAMESPACE_END