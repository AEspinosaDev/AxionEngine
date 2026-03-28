
#pragma once
#include "Axion/Graphics/IRenderer.h"
#include "Subsystems/GPUResourcePool.h"
#include "Subsystems/PipelineRegistry.h"
#include "Subsystems/RenderGraph.h"
#include "Subsystems/ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class HeadlessRenderer : public IRenderer
{

public:
    virtual ~HeadlessRenderer();

    virtual void render( RenderGraphSetupFunc setup ) override;

    virtual void destroy() override;
    virtual bool isHeadless() override;

    virtual IWindow*        getWindow() override;
    virtual void            setWindow( IWindow* /*wnd*/ ) override;
    virtual const Settings& getSettings() const override;
    virtual TextureHandle   getCurrentBackbufferHandle() const override { return TextureHandle { UINT32_MAX }; };
    virtual ulong           getTotalFrameNumber() const override { return _frameNumber; };
    virtual const uint      getTotalFramesInFlight() const override { return _FRAMES_IN_FLIGHT; };
    virtual ulong           getCurrentFrameIndex() const override;

    virtual IGPUResourcePool&  resources() override;
    virtual IShaderRegistry&   shaders() override;
    virtual IPipelineRegistry& pipelines() override;

    virtual const RHI::DeviceOwnerPtr&      getDevice() const override;
    virtual RHI::IDescriptorAllocator* getFrameDescriptorAllocator( uint frameIndex ) override;
    virtual const RHI::IGUIBackend*    getGUIBackend() const override;

    virtual std::string toString() const override;

    virtual bool instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands );

    HeadlessRenderer( const RendererSettings& settings );

private:
    RendererSettings _setts;
    // RHI -- GPU
    RHI::DeviceOwnerPtr          _device      = nullptr;
    RHI::CommandListOwnerPtr     _commandList = nullptr;
    std::vector<RHI::Fence> _frameFences;
    // SUBSYSTEMS
    GPUResourcePool  _resourcePool;   // GPU Resources
    ShaderRegistry   _shaderRegistry; // Pipelines & shaders
    PipelineRegistry _pipelineRegistry;
    RenderGraph      _renderGraph; // Render Graph
    // Query
    uint       _currentFrame = 0;
    const uint _FRAMES_IN_FLIGHT;
    ulong      _frameNumber = 0;
};

} // namespace Graphics

AXION_NAMESPACE_END