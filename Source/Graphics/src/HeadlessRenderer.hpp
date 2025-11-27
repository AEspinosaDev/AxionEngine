
#pragma once
#include "Axion/Graphics/Renderer.h"
#include "GPUFrame.hpp"
#include "Subsystems/GPUResourcePool.hpp"
#include "Subsystems/ShaderRegistry.hpp"
#include "Subsystems/PipelineRegistry.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class HeadlessRenderer : public IRenderer
{

public:
    virtual ~HeadlessRenderer();
    virtual void render() override;
    // virtual void render( const GPUSceneView& gpuScene ) override;
    virtual void destroy() override;
    virtual bool isHeadless() override;

    virtual const WindowPtr& getWindow() override;
    virtual void             setWindow( const WindowPtr& wnd ) override;
    virtual const Settings&  getSettings() const override;

    virtual IGPUResourcePool&  resources() override;
    virtual IShaderRegistry&   shaders() override;
    virtual IPipelineRegistry& pipelines() override;

    virtual const RHI::DevicePtr& getDevice() const override;

    virtual std::string toString() const override;

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
    // Query
    uint       _currentFrame = 0;
    const uint _FRAMES_IN_FLIGHT;
};

} // namespace Graphics

AXION_NAMESPACE_END