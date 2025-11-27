#include "Renderer.hpp"
#include "Axion/Graphics/Platforms/Win32.h"
#include "Axion/Graphics/RHI/DX12.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererPtr Graphics::createRenderer( const WindowPtr& wnd, const RendererSettings& settings ) {
    auto rnd = NEW_S( Renderer )( wnd, settings );
    AXION_LOG_INFO( Logger::Module::GFX, "Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, rnd->toString() );
    return rnd;
}

Renderer::Renderer( const WindowPtr& wnd, const RendererSettings& settings )
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
    _commandList    = _device->createCommandList( { .queueType = RHI::QueueType::Graphics, .numFrames = _FRAMES_IN_FLIGHT, .debugName = "Graphics Command List" } );

    // Init Resource Pool
    _resourcePool = NEW_U( GPUResourcePool )( _device.get() );
    // Init Registries
    _shaderRegistry = NEW_U( ShaderRegistry )();
    _pipelineRegistry = NEW_U( PipelineRegistry )( _device.get(), *_shaderRegistry.get() );
}

Renderer::~Renderer() {
    destroy();
}
void Renderer::render() {

    if ( _wnd->minimized() )
        return;

    // Record
    _commandList->setCurrentFrame( _currentFrame );
    _commandList->begin();
    _commandList->clearTexture( _swapchain->getSwapImages()[_currentFrame],
                                ClearValue { .color = { 0.4f, 0.6f, 0.9f, 1.0f } } );
    _commandList->barrier( _swapchain->getSwapImages()[_currentFrame],
                           RHI::ResourceState::Present );
    _commandList->end();

    // Submit + signal
    _device->executeCommandLists( { _commandList.get() },
                                  RHI::QueueType::Graphics,
                                  _frameFences[_currentFrame] );

    // Present
    _swapchain->present();

    // Handle pending resize safely
    if ( _pendingResize )
    {
        _device->waitIdle();
        auto desc = _swapchain->getDescription();
        desc.size = _wnd->getSettings().size;
        _swapchain->update( desc );
        _pendingResize = false;
    }
    // Acquire current backbuffer
    _currentFrame = _swapchain->acquireNextImage();

    _device->waitForFrame( _frameFences[_currentFrame], RHI::QueueType::Graphics );
}

// void Renderer::render( const GPUSceneView& gpuScene ) {
// }

void Renderer::destroy() {
    _device->queueWaitIdle( RHI::QueueType::Graphics, _frameFences[_currentFrame] );

    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Renderer" );
}
bool Renderer::isHeadless() {
    return false;
}

const WindowPtr& Renderer::getWindow() {
    return _wnd;
}

const RHI::DevicePtr& Renderer::getDevice() const {
    return _device;
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
    //     formatToString( outputFormat ) );
    return fmt::format(
        "Renderer Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n",
        (uint)_setts.bufferingType + 1,
        _setts.debugMode );
}

void Renderer::setWindow( const WindowPtr& wnd ) {
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
        _pendingResize = true;
}

} // namespace Graphics

AXION_NAMESPACE_END