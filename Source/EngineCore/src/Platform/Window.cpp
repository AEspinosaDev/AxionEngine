#include <Axion/Core/Platform/Window.h>
#include <Axion/Graphics/Platforms/IGLFW.h>
#include <Axion/Graphics/Platforms/IWin32.h>

AXION_NAMESPACE_BEGIN
namespace Core::Platform {

struct Window::Impl {
    Memory::OwnerPtr<Graphics::IWindow> nativeWindow;

    Graphics::PlatformType platformType;
    bool                   useVsync = false;

    mutable Settings internalSettingsBuffer;

    Impl( const Settings& startupSettings )
        : platformType( startupSettings.platformType )
        , useVsync( startupSettings.flags & WindowVSync ) {

        Graphics::IWindow::Settings s;
        s.name              = startupSettings.name;
        s.size              = startupSettings.size;
        s.fullscreen        = startupSettings.flags & WindowFullscreen;
        s.centered          = startupSettings.flags & WindowCentered;
        s.position          = startupSettings.position;
        s.iconPath          = startupSettings.iconPath;
        s.cursorPath        = startupSettings.cursorPath;
        s.style             = startupSettings.style;
        s.enableGuiInputCBs = startupSettings.flags & WindowEnableGUICallbacks;

        if ( platformType == Graphics::PlatformType::Win32 )
            nativeWindow = Graphics::createWindowForWin32( GetModuleHandle( nullptr ), s );
        else if ( platformType == Graphics::PlatformType::GLFW )
            nativeWindow = Graphics::createWindowForGLFW( s );
    }
};

Window::Window( const Settings& settings )
    : _impl( std::make_unique<Impl>( settings ) ) {

    AXION_LOG_INFO( Logger::Module::Core, "Window [{}] Created Succesfully", settings.name );
    AXION_LOG_INFO( Logger::Module::Core, "{}", toString() );
}

Window::~Window() {
    std::string wndName = _impl->nativeWindow ? _impl->nativeWindow->getSettings().name : "Closed";
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Window [{}]", wndName );
};

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
    return _impl->nativeWindow->getSettings().size;
}

void Window::setTitle( const std::string& title ) {
    _impl->nativeWindow->setTitle( title );
}

void Window::setSize( const Extent2D& size ) {
    // BYPASS: Seteamos en la nativa
    // _impl->nativeWindow->setSize( size );
}

bool Window::minimized() const {
    return _impl->nativeWindow->minimized();
}

const Window::Settings& Window::getSettings() const {

    auto nativeSetts = _impl->nativeWindow->getSettings();

    _impl->internalSettingsBuffer.name       = nativeSetts.name;
    _impl->internalSettingsBuffer.size       = nativeSetts.size;
    _impl->internalSettingsBuffer.position   = nativeSetts.position;
    _impl->internalSettingsBuffer.iconPath   = nativeSetts.iconPath;
    _impl->internalSettingsBuffer.cursorPath = nativeSetts.cursorPath;
    _impl->internalSettingsBuffer.style      = nativeSetts.style;

    _impl->internalSettingsBuffer.platformType = _impl->platformType;

    _impl->internalSettingsBuffer.flags = WindowNone;
    if ( nativeSetts.centered )
        _impl->internalSettingsBuffer.flags |= WindowCentered;
    if ( nativeSetts.fullscreen )
        _impl->internalSettingsBuffer.flags |= WindowFullscreen;
    if ( _impl->useVsync )
        _impl->internalSettingsBuffer.flags |= WindowVSync;
    if ( nativeSetts.enableGuiInputCBs )
        _impl->internalSettingsBuffer.flags |= WindowEnableGUICallbacks;

    return _impl->internalSettingsBuffer;
}

Graphics::PlatformType Window::getPlatformType() const {
    return _impl->platformType;
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

std::string Window::toString() const {
    const auto& currentSettings = getSettings();

    std::string pltName = "Unknown";
    if ( currentSettings.platformType == Graphics::PlatformType::Win32 )
        pltName = "Win32";
    else if ( currentSettings.platformType == Graphics::PlatformType::GLFW )
        pltName = "GLFW";

    return fmt::format(
        "Window Settings (Live State):\n"
        "  Name: {}\n"
        "  Platform: {}\n"
        "  VSync: {}\n"
        "  Fullscreen: {}\n"
        "  Position: \n"
        "    x = {} \n"
        "    y = {} \n"
        "  Size: \n"
        "    Width = {} \n"
        "    Height = {} \n"
        "  Centered: {}\n"
        "  Icon Path: {}\n"
        "  Cursor Path: {}\n",
        currentSettings.name,
        pltName,
        currentSettings.flags & WindowEnableGUICallbacks ? "Yes" : "No",
        currentSettings.flags & WindowFullscreen ? "Yes" : "No",
        currentSettings.position.x,
        currentSettings.position.y,
        currentSettings.size.width,
        currentSettings.size.height,
        currentSettings.flags & WindowCentered ? "Yes" : "No",
        currentSettings.iconPath,
        currentSettings.cursorPath );
}

} // namespace Core::Platform
AXION_NAMESPACE_END