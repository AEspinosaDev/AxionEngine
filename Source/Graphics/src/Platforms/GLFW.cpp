#include "GLFW.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {

WindowPtr createWindowForGLFW( const WindowSettings& settings ) {
    return NEW_U( GLFWWindow )( settings );
}

GLFWWindow::GLFWWindow( const Settings& settings )
    : _setts( settings )
    , _windowedSizeCache( settings.size ) {

    glfwInit();
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API ); // Set for Vulkan/DX12 context
    glfwWindowHint( GLFW_RESIZABLE, true );
    _hWnd = glfwCreateWindow( _setts.size.width, _setts.size.height, _setts.name.c_str(), nullptr, nullptr );

    if ( !_hWnd )
    {
        glfwTerminate();
        AXION_LOG_ERROR( Logger::Module::GFX, "[FATAL] Failed to create GLFW window" );
    }

    _initialized = true;

    AXION_LOG_INFO( Logger::Module::GFX, "Window for Platform GLFW Created Successfully" );
    AXION_LOG_INFO( Logger::Module::GFX, "Window Size: \n Width = {} \n Height = {} \n", _setts.size.width, _setts.size.height );

    if ( _setts.centered )
    {
        GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode    = glfwGetVideoMode( monitor );

        int monitorX, monitorY;
        glfwGetMonitorPos( monitor, &monitorX, &monitorY );

        int winWidth  = _setts.size.width;
        int winHeight = _setts.size.height;

        int posX = monitorX + ( mode->width - winWidth ) / 2;
        int posY = monitorY + ( mode->height - winHeight ) / 2;

        _setts.position = { (uint)posX, (uint)posY };
    }
    glfwSetWindowPos( _hWnd, (int)_setts.position.x, (int)_setts.position.y );
    glfwSetWindowUserPointer( _hWnd, this );

    setCallbacksFunctions();
}

GLFWWindow::~GLFWWindow() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Platform GLFW Window" );
    glfwDestroyWindow( _hWnd );
}

bool GLFWWindow::processMessages() {
    glfwPollEvents();
    _shouldClose = glfwWindowShouldClose( _hWnd );
    return !_shouldClose;
}

void GLFWWindow::setFullscreen( bool fullscreen ) {

    _setts.fullscreen = fullscreen;
    if ( !_setts.fullscreen )
    {
        glfwSetWindowMonitor( _hWnd,
                              NULL,
                              (int)_setts.position.x,
                              (int)_setts.position.y,
                              _windowedSizeCache.width,
                              _windowedSizeCache.height,
                              GLFW_DONT_CARE );
        _setts.size = _windowedSizeCache;
    } else
    {
        const GLFWvidmode* mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
        glfwSetWindowMonitor( _hWnd, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate );
        _setts.size = { (uint)mode->width, (uint)mode->height };
    }

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize( _hWnd, &fbw, &fbh );
    while ( fbw == 0 || fbh == 0 )
    {
        glfwWaitEvents();
        glfwGetFramebufferSize( _hWnd, &fbw, &fbh );
    }

    _setts.size = { (uint)fbw, (uint)fbh };

    Event::WindowResizeEvent evt( _hWnd, fbw, fbh );
    _onResize.dispatch( evt );
}

bool GLFWWindow::minimized() const {
    return _minimized;
}

void GLFWWindow::setTitle( const std::string& title ) {
    glfwSetWindowTitle( _hWnd, title.c_str() );
}

RHI::NativeObject GLFWWindow::getNativeObject() {
    auto native = RHI::NativeObject( RHI::ObjectTypes::GLFW_Window, _hWnd );
    return native;
}

void GLFWWindow::setCallbacksFunctions() {
    // --- Keyboard ---
    glfwSetKeyCallback( _hWnd, []( GLFWwindow* w, int key, int scancode, int action, int mods ) {
        GLFWWindow* instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );

        if ( key == GLFW_KEY_F11 && action == GLFW_PRESS )
            instance->setFullscreen( !instance->_setts.fullscreen );
        if ( action == GLFW_PRESS || action == GLFW_REPEAT )
        {
            Event::KeyEvent evt( w,  mapGLFWKey( key ), true );
            instance->_onKey.dispatch( evt );
        } else if ( action == GLFW_RELEASE )
        {
            Event::KeyEvent evt( w,  mapGLFWKey( key ), false );
            instance->_onKey.dispatch( evt );
        }
    } );

    // --- Mouse buttons ---
    glfwSetMouseButtonCallback( _hWnd, []( GLFWwindow* w, int button, int action, int mods ) {
        GLFWWindow*             instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        Event::MouseButtonEvent evt( w, (uint)button, action == GLFW_PRESS );
        instance->_onMouseButton.dispatch( evt );
    } );

    // --- Mouse movement ---
    glfwSetCursorPosCallback( _hWnd, []( GLFWwindow* w, double x, double y ) {
        GLFWWindow*           instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        Event::MouseMoveEvent evt( w, (int)x, (int)y );
        instance->_onMouseMove.dispatch( evt );
    } );

    // --- Mouse scroll ---
    glfwSetScrollCallback( _hWnd, []( GLFWwindow* w, double x, double y ) {
        GLFWWindow*             instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        Event::MouseScrollEvent evt( w, (float)y );
        instance->_onMouseScroll.dispatch( evt );
    } );

    // --- Resize ---
    glfwSetFramebufferSizeCallback( _hWnd, []( GLFWwindow* w, int width, int height ) {
        GLFWWindow* instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );

        if ( width == 0 || height == 0 )
        {
            instance->_minimized = true;
            return;
        }

        instance->_minimized  = false;
        instance->_setts.size = { (uint)width, (uint)height };
        Event::WindowResizeEvent evt( w, (uint)width, (uint)height );
        instance->_onResize.dispatch( evt );
    } );

    // --- Window close ---
    glfwSetWindowCloseCallback( _hWnd, []( GLFWwindow* w ) {
        GLFWWindow* instance   = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        instance->_shouldClose = true;
        Event::WindowCloseEvent evt( w );
        instance->_onClose.dispatch( evt );
    } );
}

} // namespace Graphics

AXION_NAMESPACE_END