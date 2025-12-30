#include <Axion/Core/Platform/Window.h>
#include <Axion/Graphics/Platforms/GLFW.h>
#include <Axion/Graphics/Platforms/Win32.h>

AXION_NAMESPACE_BEGIN
namespace Core::Platform {

struct Window::Impl {
    std::unique_ptr<Graphics::IWindow> nativeWindow;
    Settings                           setts;

    Impl( const Settings& settings )
        : setts( settings ) {

        Graphics::IWindow::Settings s;
        s.name       = setts.name;
        s.size       = setts.size;
        s.fullscreen = setts.fullscreen;
        s.centered   = setts.centered;
        s.position   = setts.position;
        s.iconPath   = setts.iconPath;
        s.cursorPath = setts.cursorPath;
        s.style      = setts.style;

        if ( setts.platformType == Graphics::PlatformType::Win32 )
            nativeWindow = Graphics::createWindowForWin32( GetModuleHandle( nullptr ), s );
        else if ( setts.platformType == Graphics::PlatformType::GLFW )
            nativeWindow = Graphics::createWindowForGLFW( s );
    }
};

Window::Window( const Settings& settings )
    : _impl( std::make_unique<Impl>( settings ) ) {}

Window::~Window() = default;

bool Window::update() {
    return _impl->nativeWindow->processMessages();
}

void Window::setFullscreen( bool fullscreen ) {
    _impl->nativeWindow->setFullscreen( fullscreen );
}

bool Window::shouldClose() const {
    return _impl->nativeWindow->shouldClose();
}

Extent2D Window::getSize() const {
    return _impl->setts.size;
}

void Window::setSize( const Extent2D& size ) {
}

bool Window::minimized() const {
    return _impl->nativeWindow->minimized();
}
const Window::Settings& Window::getSettings() const {
    return _impl->setts;
}

Graphics::PlatformType Window::getPlatformType() const {
    return _impl->setts.platformType;
}

Graphics::IWindow* Window::getNativeWindow() const {
    return _impl->nativeWindow.get();
}

Event::EventDispatcher<Event::WindowResizeEvent>& Window::onResize() {
    return _impl->nativeWindow->onResize();
}

Event::EventDispatcher<Event::WindowCloseEvent>& Window::onClose() {
    return _impl->nativeWindow->onClose();
}

Event::EventDispatcher<Event::KeyEvent>& Window::onKey() {
    return _impl->nativeWindow->onKey();
}

Event::EventDispatcher<Event::MouseButtonEvent>& Window::onMouseButton() {
    return _impl->nativeWindow->onMouseButton();
}

Event::EventDispatcher<Event::MouseMoveEvent>& Window::onMouseMove() {
    return _impl->nativeWindow->onMouseMove();
}

Event::EventDispatcher<Event::MouseScrollEvent>& Window::onMouseScroll() {
    return _impl->nativeWindow->onMouseScroll();
}

} // namespace Core::Platform
AXION_NAMESPACE_END