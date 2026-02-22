
#include "HeadlessRenderer.hpp"
#include "Axion/Graphics/RHI/DX12.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererPtr Graphics::createHeadlessRenderer( const RendererSettings& settings ) {
    auto rnd = NEW_U( HeadlessRenderer )( settings );
    AXION_LOG_INFO( Logger::Module::GFX, "Headless Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, "{}", rnd->toString() );

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

    // Init Command List
    _commandList = _device->createCommandList( { .queueType = RHI::QueueType::Graphics, .numFrames = _FRAMES_IN_FLIGHT, .debugName = "Graphics Command List" } );

    // Init Resource Pool
    _resourcePool = NEW_U( GPUResourcePool )( _device.get() );
    // Init Registries
    _shaderRegistry   = NEW_U( ShaderRegistry )();
    _pipelineRegistry = NEW_U( PipelineRegistry )( _device.get(), *_shaderRegistry.get() );
    // Init Render Graph
    RenderGraphDesc RGDesc = {
        .framesInFlight        = _FRAMES_IN_FLIGHT,
        .passDataAllocSize     = _setts.RGAllocSize,
        .desciptorSetAllocSize = _setts.RGDescriptorsPerFrame,
        .resourceTTL           = (uint)_setts.GCMode,
    };
    _renderGraph = NEW_U( RenderGraph )( _device.get(), *_resourcePool.get(), *_pipelineRegistry.get(), RGDesc );
}

HeadlessRenderer::~HeadlessRenderer() {
    destroy();
}
void HeadlessRenderer::render( RenderGraphSetupFunc setup ) {
    // Record
    _commandList->setCurrentFrame( _currentFrame );
    _commandList->begin();

    _renderGraph->execute( setup, _commandList.get() );

    _commandList->end();

    // Submit + signal
    _device->executeCommandLists( { _commandList },
                                  RHI::QueueType::Graphics,
                                  _frameFences[_currentFrame] );

    // Acquire current backbuffer
    _currentFrame = ( _currentFrame + 1 ) % _FRAMES_IN_FLIGHT;

    _device->waitForFrame( _frameFences[_currentFrame], RHI::QueueType::Graphics );
}

void HeadlessRenderer::destroy() {
    _device->queueWaitIdle( RHI::QueueType::Graphics, _frameFences[_currentFrame] );
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Headless Renderer" );
}
bool HeadlessRenderer::isHeadless() {
    return true;
}

IWindow* HeadlessRenderer::getWindow() {
    return nullptr;
}

const RHI::DevicePtr& HeadlessRenderer::getDevice() const {
    return _device;
}

RHI::IDescriptorAllocator* HeadlessRenderer::getFrameDescriptorAllocator( uint frameIndex ) {
    return _renderGraph->getDescriptorAllocator( frameIndex );
}

std::string HeadlessRenderer::toString() const {
    return fmt::format(
        "Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n",
        (uint)_setts.bufferingType + 1,
        _setts.debugMode );
}

bool HeadlessRenderer::instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands ) {
    _device->oneTimeSubmit( commands );
    return true;
}

void HeadlessRenderer::setWindow( IWindow* wnd ) {
    // AXION_LOG_WARN(Logger::Module::GFX, "Renderer [{}] is headless, it does not need a window.")
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

ulong HeadlessRenderer::getCurrentFrameIndex() const {
    return _currentFrame;
}

} // namespace Graphics

AXION_NAMESPACE_END