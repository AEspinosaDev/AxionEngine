#include "Renderer.hpp"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/RHI/DX12.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererPtr Graphics::createRenderer( IWindow* wnd, const RendererSettings& settings ) {
    auto rnd = NEW_U( Renderer )( wnd, settings );
    AXION_LOG_INFO( Logger::Module::GFX, "Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, rnd->toString() );
    return rnd;
}

Renderer::Renderer( IWindow* wnd, const RendererSettings& settings )
    : _wnd( wnd )
    , _setts( settings )
    , _FRAMES_IN_FLIGHT( static_cast<uint>( settings.bufferingType ) + 1 ) {

    _frameFences.resize( _FRAMES_IN_FLIGHT );

    // Per Graphics API Device Creation
    switch ( _setts.gfxApi )
    {
        case API::DirectX12:
            RHI::DX12DeviceDesc desc { .enableDebugLayer = _setts.debugMode };
            _device = RHI::createDX12Device( desc );
            break;
            // case GraphicsAPI::Vulkan:

            //     break;

            // default:
            //     break;
    }

    AXION_LOG_ASSERT( _wnd, Logger::Module::GFX, "Window is NULL | Renderer needs Window. If no window needed, use Headless Renderer" );
    _swapchain      = _device->createSwapchain( wnd->getNativeObject(), { .size = wnd->getSettings().size, .imageCount = _FRAMES_IN_FLIGHT, .presentMode = settings.presentMode } );
    _resizeCbHandle = _wnd->onResize().subscribe( [this]( const Event::WindowResizeEvent& e ) { this->windowCallback( { e.width, e.height } ); } );
    // Init Command List
    _commandList = _device->createCommandList( { .queueType = RHI::QueueType::Graphics, .numFrames = _FRAMES_IN_FLIGHT, .debugName = "Graphics Command List" } );

    // Init Resource Pool
    _resourcePool = NEW_U( GPUResourcePool )( _device.get() );
    generateSwapchainHandles();
    // Init Registries
    _shaderRegistry   = NEW_U( ShaderRegistry )();
    _pipelineRegistry = NEW_U( PipelineRegistry )( _device.get(), *_shaderRegistry.get() );
    // Init Render Graph
    RenderGraphDesc RGDesc = {
        .framesInFlight        = _FRAMES_IN_FLIGHT,
        .passDataAllocSize     = _setts.RGAllocSize,
        .desciptorSetAllocSize = _setts.RGDescriptorsPerFrame,
        .sbtAllocSize          = _setts.RGAllocSBTSize,
        .transientAllocSize    = _setts.RGTransientAllocSize,
        .resourceTTL           = (uint)_setts.GCMode,
        .autoSync              = _setts.autoSync };
    _renderGraph = NEW_U( RenderGraph )( _device.get(), *_resourcePool.get(), *_pipelineRegistry.get(), RGDesc );
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
                _resourcePool->destroyTexture( handle );
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

    _renderGraph->execute( setup, _commandList.get() );

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

const RHI::DevicePtr& Renderer::getDevice() const {
    return _device;
}

TextureHandle Renderer::getCurrentBackbufferHandle() const {
    return _swapchainHandles[_currentFrame];
}

ulong Renderer::getCurrentFrameIndex() const {
    return _currentFrame;
}

std::string Renderer::toString() const {
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
    return fmt::format(
        "Renderer Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n",
        (uint)_setts.bufferingType + 1,
        _setts.debugMode );
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
    return *_resourcePool.get();
}

IShaderRegistry& Renderer::shaders() {
    return *_shaderRegistry.get();
}

IPipelineRegistry& Renderer::pipelines() {
    return *_pipelineRegistry.get();
}

void Renderer::windowCallback( const Extent2D& newSize ) {
    if ( newSize.width > 0 || newSize.height > 0 )
    {
        _pendingResize = true;
    }
}
void Renderer::generateSwapchainHandles() {
    _swapchainHandles.clear();
    auto images = _swapchain->getSwapImages();

    for ( size_t i = 0; i < images.size(); ++i )
    {
        auto handle = _resourcePool->registerExternalTexture( images[i], "Backbuffer_" + std::to_string( i ) );
        _swapchainHandles.push_back( handle );
    }
}
} // namespace Graphics

AXION_NAMESPACE_END