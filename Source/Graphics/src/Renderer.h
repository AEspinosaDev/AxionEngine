#pragma once
#include "Axion/Graphics/IRenderer.h"
#include "Subsystems/GPUResourcePool.h"
#include "Subsystems/PipelineRegistry.h"
#include "Subsystems/RenderGraph.h"
#include "Subsystems/ShaderRegistry.h"

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

    virtual IWindow*        getWindow() override;
    virtual void            setWindow( IWindow* wnd ) override;
    virtual const Settings& getSettings() const override;
    virtual TextureHandle   getCurrentBackbufferHandle() const override;
    virtual u64           getTotalFrameNumber() const override { return _frameNumber; }
    virtual const u32      getTotalFramesInFlight() const override { return _FRAMES_IN_FLIGHT; };
    virtual u64           getCurrentFrameIndex() const override;

    virtual const RHI::DeviceOwnerPtr& getDevice() const override;
    virtual RHI::IDescriptorAllocator* getFrameDescriptorAllocator( u32 frameIndex ) override;
    virtual const RHI::IGUIBackend*    getGUIBackend() const override;

    virtual bool         isHeadless() override;
    virtual void         destroy() override;
    virtual STLW::String toString() const override;

    virtual bool instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands );

    Renderer( IWindow* wnd, const RendererSettings& settings );

private:
    void windowCallback( const Extent2D& newSize );
    void generateSwapchainHandles();

    RendererSettings _setts;
    // RHI -- GPU
    RHI::DeviceOwnerPtr      _device      = nullptr;
    RHI::CommandListOwnerPtr _commandList = nullptr;
    std::vector<RHI::Fence>  _frameFences;

    // SUBSYSTEMS
    GPUResourcePool  _resourcePool;   // GPU Resources
    ShaderRegistry   _shaderRegistry; // Pipelines & shaders
    PipelineRegistry _pipelineRegistry;
    RenderGraph      _renderGraph; // Render Graph

    // Window Related
    IWindow*                                                                        _wnd            = nullptr;
    std::unique_ptr<Event::EventDispatcher<Event::WindowResizeEvent>::Subscription> _resizeCbHandle = nullptr;
    bool                                                                            _pendingResize  = false;
    RHI::SwapchainOwnerPtr                                                          _swapchain      = nullptr;
    std::vector<TextureHandle>                                                      _swapchainHandles;
    // GUI Backend (IMGUI)
    RHI::GUIBackendOwnerPtr _guiBackend = nullptr;
    // Query
    u32       _currentFrame = 0;
    const u32 _FRAMES_IN_FLIGHT;
    u64      _frameNumber = 0;
};

} // namespace Graphics

AXION_NAMESPACE_END