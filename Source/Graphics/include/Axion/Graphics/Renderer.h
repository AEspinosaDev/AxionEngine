#pragma once
#include "Axion/Graphics/Platforms/Window.h"
#include "Axion/Graphics/RHI/Device.h"
#include "Axion/Graphics/Subsystems/GPUResourcePool.h"
#include "Axion/Graphics/Subsystems/PipelineRegistry.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

DEFINE_SHARED_PTR_FOR_TYPE( IRenderer, Renderer )

class IRenderer
{
public:
    struct Settings {
        API           gfxApi        = API::DirectX12;
        BufferingType bufferingType = BufferingType::Double;
        bool          debugMode     = true;
        PresentMode   presentMode   = PresentMode::Vsync;  // Only if windowed
        Format        outputFormat  = Format::RGBA8_UNORM; // Only if windowed
    };

    virtual ~IRenderer() = default;

    virtual void render() = 0;

    // Subsystems
    virtual IGPUResourcePool&  resources() = 0;
    virtual IShaderRegistry&   shaders()   = 0;
    virtual IPipelineRegistry& pipelines() = 0;

    // Getters
    virtual const WindowPtr&      getWindow()                       = 0;
    virtual void                  setWindow( const WindowPtr& wnd ) = 0;
    virtual const Settings&       getSettings() const               = 0;
    virtual const RHI::DevicePtr& getDevice() const                 = 0;

    // Query
    virtual void        destroy()        = 0;
    virtual std::string toString() const = 0;
    virtual bool        isHeadless()     = 0;
};

typedef IRenderer::Settings RendererSettings;

RendererPtr createRenderer( const WindowPtr& wnd, const RendererSettings& settings = {} );
RendererPtr createHeadlessRenderer( const RendererSettings& settings = {} );

} // namespace Graphics

AXION_NAMESPACE_END
