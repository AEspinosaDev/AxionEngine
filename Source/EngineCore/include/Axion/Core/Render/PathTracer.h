
#pragma once
#include <Axion/Core/Platform/Window.h>
#include <Axion/Core/Render/Renderer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class IPathTracer : public IRenderer
{
public:
    struct Settings {
    };

    virtual Settings getSettings()                           = 0;
    virtual void     setSettings( const Settings& settings ) = 0;
};

typedef IPathTracer::Settings PathTracerSettings;

RendererPtr createPathTracer( Platform::Window* wnd, PathTracerSettings settings = {} );
};

} // namespace Core::Render

AXION_NAMESPACE_END