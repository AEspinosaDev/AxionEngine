#pragma once
#include "Axion/Graphics/Renderer.h"
#include "GPUFrame.hpp"
#include "Subsystems/GPUResourcePool.hpp"
#include "Subsystems/PipelineRegistry.hpp"
#include "Subsystems/RenderGraph.hpp"
#include "Subsystems/ShaderRegistry.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class Renderer : public IRenderer
{

public:
    virtual ~Renderer();

    virtual void render( RenderGraphSetupFunc setup ) override;

    virtual IGPUResourcePool&  resources() override;
    virtual IShaderRegistry&   shaders() override;
    virtual IPipelineRegistry& pipelines() override;

    virtual IWindow*              getWindow() override;
    virtual void                  setWindow( IWindow* wnd ) override;
    virtual const Settings&       getSettings() const override;
    virtual const RHI::DevicePtr& getDevice() const override;
    virtual TextureHandle         getCurrentBackbufferHandle() const override;
    virtual ulong                 getTotalFrameNumber() const override { return _frameNumber; }
    virtual const uint            getTotalFramesInFlight() const override { return _FRAMES_IN_FLIGHT; };
    virtual ulong                 getCurrentFrameIndex() const override;

    virtual bool        isHeadless() override;
    virtual void        destroy() override;
    virtual std::string toString() const override;

    virtual bool instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands );

    Renderer( IWindow* wnd, const RendererSettings& settings );

private:
    void windowCallback( const Extent2D& newSize );
    void generateSwapchainHandles();

    RendererSettings _setts;
    // RHI -- GPU
    RHI::DevicePtr          _device      = nullptr;
    RHI::CommandListPtr     _commandList = nullptr;
    std::vector<RHI::Fence> _frameFences;
    // GPU Resources
    GPUResourcePoolPtr    _resourcePool = nullptr;
    std::vector<GPUFrame> _frames;
    // Pipelines & shaders
    ShaderRegistryPtr   _shaderRegistry   = nullptr;
    PipelineRegistryPtr _pipelineRegistry = nullptr;
    // Render Graph
    RenderGraphPtr _renderGraph = nullptr;
    // Window Related
    IWindow*                                                                        _wnd            = nullptr;
    std::unique_ptr<Event::EventDispatcher<Event::WindowResizeEvent>::Subscription> _resizeCbHandle = nullptr;
    bool                                                                            _pendingResize  = false;
    RHI::SwapchainPtr                                                               _swapchain      = nullptr;
    std::vector<TextureHandle>                                                      _swapchainHandles;
    // Query
    uint       _currentFrame = 0;
    const uint _FRAMES_IN_FLIGHT;
    ulong      _frameNumber = 0;
};

} // namespace Graphics

AXION_NAMESPACE_END