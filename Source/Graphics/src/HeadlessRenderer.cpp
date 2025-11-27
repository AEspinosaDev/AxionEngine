
#include "HeadlessRenderer.hpp"
#include "Axion/Graphics/RHI/DX12.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererPtr Graphics::createHeadlessRenderer( const RendererSettings& settings ) {
    auto rnd = NEW_S( HeadlessRenderer )( settings );
    AXION_LOG_INFO( Logger::Module::GFX, "Headless Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, rnd->toString() );
    return rnd;
}

HeadlessRenderer::HeadlessRenderer( const RendererSettings& settings )
    : _setts( settings )
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

    _commandList = _device->createCommandList( { .queueType = RHI::QueueType::Graphics, .numFrames = _FRAMES_IN_FLIGHT } );
#ifdef RAIKO_DEBUG
    _commandList->setDebugName( "Graphics Command List" );
#endif
}

HeadlessRenderer::~HeadlessRenderer() {
    destroy();
}
void HeadlessRenderer::render() {

    // Record
    _commandList->setCurrentFrame( _currentFrame );
    _commandList->begin();

    _commandList->end();

    // Submit + signal
    _device->executeCommandLists( { _commandList },
                                  RHI::QueueType::Graphics,
                                  _frameFences[_currentFrame] );

    // Acquire current backbuffer
    _currentFrame = ( _currentFrame + 1 ) % _FRAMES_IN_FLIGHT;

    _device->waitForFrame( _frameFences[_currentFrame], RHI::QueueType::Graphics );
}

// void HeadlessRenderer::render( const GPUSceneView& gpuScene ) {
// }

void HeadlessRenderer::destroy() {
    _device->queueWaitIdle( RHI::QueueType::Graphics, _frameFences[_currentFrame] );
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Headless Renderer" );
}
bool HeadlessRenderer::isHeadless() {
    return true;
}

const WindowPtr& HeadlessRenderer::getWindow() {
    return nullptr;
}

const RHI::DevicePtr& HeadlessRenderer::getDevice() const {
    return _device;
}

std::string HeadlessRenderer::toString() const {
    return fmt::format(
        "Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n",
        (uint)_setts.bufferingType + 1,
        _setts.debugMode );
}

void HeadlessRenderer::setWindow( const WindowPtr& wnd ) {
}

const RendererSettings& HeadlessRenderer::getSettings() const {
    return _setts;
}

IGPUResourcePool& HeadlessRenderer::resources() {
    return *_resourcePool.get();
}

IShaderRegistry& HeadlessRenderer::shaders() {
    return *_shaderRegistry.get();
}

IPipelineRegistry& HeadlessRenderer::pipelines() {
    return *_pipelineRegistry.get();
}

} // namespace Graphics

AXION_NAMESPACE_END