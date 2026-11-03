
#include "HeadlessRenderer.h"
#include "Axion/Graphics/RHI/DX12/IDX12Device.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

RendererOwnerPtr Graphics::createHeadlessRenderer( const RendererSettings& settings ) {

    auto rnd = Memory::makeOwned<HeadlessRenderer>( settings );

    AXION_LOG_INFO( Logger::Module::GFX, "Headless Renderer Created Succesfully" );
    AXION_LOG_INFO( Logger::Module::GFX, "{}", rnd->toString() );

    return rnd;
}

HeadlessRenderer::HeadlessRenderer( const RendererSettings& settings )
    : _setts( settings )
    , _FRAMES_IN_FLIGHT( static_cast<u32>( settings.bufferingType ) + 1 ) {

    _frameFences.resize( _FRAMES_IN_FLIGHT );

    AXION_LOG_ASSERT( _setts.RDGmaxAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.host.maxPersistentAlloc,
                      Logger::Module::GFX,
                      "RenderGraph persistent allocation exceeds Host memory budget." );
    AXION_LOG_ASSERT( _setts.maxSBTAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.device.maxUploadAlloc,
                      Logger::Module::GFX,
                      "SBT allocation exceeds Device Upload budget." );
    AXION_LOG_ASSERT( _setts.maxStagingAlloc * _FRAMES_IN_FLIGHT <= _setts.memory.device.maxUploadAlloc,
                      Logger::Module::GFX,
                      "Transient allocation exceeds Device Upload budget." );

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
        .passDataAllocSize = _setts.RDGmaxAlloc,
        .resourceTTL       = (u32)_setts.GCMode,
        .autoSync          = _setts.autoSync };
    _renderGraph.initialize( ctx, RGDesc );
    _renderGraph.initialize( ctx, RGDesc );
}

HeadlessRenderer::~HeadlessRenderer() {
    destroy();
}
void HeadlessRenderer::render( RenderGraphSetupFunc setup ) {
    // Reset Allocators
    _descriptorAllocators[_currentFrame]->reset();
    _sbtAllocators[_currentFrame]->reset();
    _transientDataAllocators[_currentFrame].reset();
    // Reset Command Buffer
    _commandList->setCurrentFrame( _currentFrame );
    _commandList->begin();

    // Record
    _renderGraph.execute( setup,
                          _commandList.get(),
                          _descriptorAllocators[_currentFrame].get(),
                          _sbtAllocators[_currentFrame].get(),
                          &_transientDataAllocators[_currentFrame] );

    _commandList->end();

    // Submit + signal
    _device->executeCommandLists( { _commandList.get() },
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
    // AXION_LOG_WARN(Logger::Module::GFX, "Renderer [{}] is headless, it does not have a window.", _setts.name);
    return nullptr;
}

const RHI::DeviceOwnerPtr& HeadlessRenderer::getDevice() const {
    return _device;
}

RHI::IDescriptorAllocator* const HeadlessRenderer::getDescriptorAllocator() {
    return _persistentDescriptorAllocator.get();
}

const RHI::IGUIBackend* HeadlessRenderer::getGUIBackend() const {
    // AXION_LOG_WARN( Logger::Module::GFX, "Renderer [{}] is headless, it does not have a GUI backend.", _setts. );
    return nullptr;
}

STLW::String HeadlessRenderer::toString() const {
    return static_cast<STLW::String>( fmt::format(
        "Renderer Settings:\n"
        "  Buffering Type: {}\n"
        "  Debug Mode: {}\n",
        (u32)_setts.bufferingType + 1,
        _setts.debugMode ) );
}

bool HeadlessRenderer::instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands ) {
    _device->oneTimeSubmit( commands );
    return true;
}

void HeadlessRenderer::setWindow( IWindow* /*wnd*/ ) {
    // AXION_LOG_WARN(Logger::Module::GFX, "Renderer [{}] is headless, it does not need a window.", _setts.name);
}

const RendererSettings& HeadlessRenderer::getSettings() const {
    return _setts;
}

IGPUResourcePool& HeadlessRenderer::resources() {
    return _resourcePool;
}

IShaderRegistry& HeadlessRenderer::shaders() {
    return _shaderRegistry;
}

IPipelineRegistry& HeadlessRenderer::pipelines() {
    return _pipelineRegistry;
}

u32 HeadlessRenderer::getCurrentFrameIndex() const {
    return _currentFrame;
}

} // namespace Graphics

AXION_NAMESPACE_END