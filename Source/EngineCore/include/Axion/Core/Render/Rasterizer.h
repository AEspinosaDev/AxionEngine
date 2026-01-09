
#pragma once
#include <Axion/Core/Platform/Window.h>
#include <Axion/Core/Render/Renderer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class IRasterizer : public IRenderer
{
public:
    struct Settings {
        Graphics::Format depthFormat = Graphics::Format::D32;
        CommonSettings   common {};
        MemoryBudget     memory {};
    };

    virtual Settings getSettings() const                     = 0;
    virtual void     setSettings( const Settings& settings ) = 0;
};

typedef IRasterizer::Settings RasterizerSettings;

RendererPtr createRasterizer( Platform::Window* wnd, const RasterizerSettings& settings = {} );

} // namespace Core::Render

AXION_NAMESPACE_END