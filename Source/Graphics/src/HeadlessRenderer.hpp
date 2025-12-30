
#pragma once
#include "Axion/Graphics/Renderer.h"
#include "GPUFrame.hpp"
#include "Subsystems/GPUResourcePool.hpp"
#include "Subsystems/PipelineRegistry.hpp"
#include "Subsystems/RenderGraph.hpp"
#include "Subsystems/ShaderRegistry.hpp"

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
    virtual void            setWindow( IWindow* wnd ) override;
    virtual const Settings& getSettings() const override;
    virtual TextureHandle   getCurrentBackbufferHandle() const override { return TextureHandle { UINT32_MAX }; };
    virtual ulong           getTotalFrameNumber() const override { return _frameNumber; };
    virtual ulong           getCurrentFrameIndex() const override;

    virtual IGPUResourcePool&  resources() override;
    virtual IShaderRegistry&   shaders() override;
    virtual IPipelineRegistry& pipelines() override;

    virtual const RHI::DevicePtr& getDevice() const override;

    virtual std::string toString() const override;

    virtual bool instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands );

    HeadlessRenderer( const RendererSettings& settings );

private:
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
    // Query
    uint       _currentFrame = 0;
    const uint _FRAMES_IN_FLIGHT;
    ulong      _frameNumber = 0;
};

} // namespace Graphics

AXION_NAMESPACE_END