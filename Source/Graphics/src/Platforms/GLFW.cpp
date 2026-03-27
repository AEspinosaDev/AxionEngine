#include "GLFW.h"
#include <backends/imgui_impl_glfw.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

WindowOwnerPtr createWindowForGLFW( const WindowSettings& settings ) {
    return Memory::makeOwned<GLFWWindow>( settings );
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
        if ( instance->onGUIKey( w, key, scancode, action, mods ) )
            return;

        if ( key == GLFW_KEY_F11 && action == GLFW_PRESS )
            instance->setFullscreen( !instance->_setts.fullscreen );
        if ( action == GLFW_PRESS || action == GLFW_REPEAT )
        {
            Event::KeyEvent evt( w, mapGLFWKey( key ), true );
            instance->_onKey.dispatch( evt );
        } else if ( action == GLFW_RELEASE )
        {
            Event::KeyEvent evt( w, mapGLFWKey( key ), false );
            instance->_onKey.dispatch( evt );
        }
    } );

    glfwSetCharCallback( _hWnd, []( GLFWwindow* w, unsigned int c ) {
        GLFWWindow* instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        if ( instance->onGUIChar( w, c ) )
            return;
    } );

    // --- Mouse buttons ---
    glfwSetMouseButtonCallback( _hWnd, []( GLFWwindow* w, int button, int action, int mods ) {
        GLFWWindow*             instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        if ( instance->onGUIMouseButton( w, button, action, mods ) )
            return;
        Event::MouseButtonEvent evt( w, (uint)button, action == GLFW_PRESS );
        instance->_onMouseButton.dispatch( evt );
    } );

    // --- Mouse movement ---
    glfwSetCursorPosCallback( _hWnd, []( GLFWwindow* w, double x, double y ) {
        GLFWWindow*           instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        if ( instance->onGUIMouseMove( w, x, y ) )
            return;
        Event::MouseMoveEvent evt( w, (int)x, (int)y );
        instance->_onMouseMove.dispatch( evt );
    } );

    // --- Mouse scroll ---
    glfwSetScrollCallback( _hWnd, []( GLFWwindow* w, double x, double y ) {
        GLFWWindow*             instance = static_cast<GLFWWindow*>( glfwGetWindowUserPointer( w ) );
        if ( instance->onGUIMouseScroll( w, x, y ) )
            return;
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

bool GLFWWindow::onGUIKey( GLFWwindow* window, int key, int scancode, int action, int mods ) {
    if ( !ImGui::GetCurrentContext() )
        return false;

    ImGui_ImplGlfw_KeyCallback( window, key, scancode, action, mods );
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool GLFWWindow::onGUIChar( GLFWwindow* window, unsigned int c ) {
    if ( !ImGui::GetCurrentContext() )
        return false;

    ImGui_ImplGlfw_CharCallback( window, c );
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool GLFWWindow::onGUIMouseButton( GLFWwindow* window, int button, int action, int mods ) {
    if ( !ImGui::GetCurrentContext() )
        return false;

    ImGui_ImplGlfw_MouseButtonCallback( window, button, action, mods );
    return ImGui::GetIO().WantCaptureMouse;
    return false;
}

bool GLFWWindow::onGUIMouseMove( GLFWwindow* window, double x, double y ) {
    if ( !ImGui::GetCurrentContext() )
        return false;

    ImGui_ImplGlfw_CursorPosCallback( window, x, y );
    return ImGui::GetIO().WantCaptureMouse;
}

bool GLFWWindow::onGUIMouseScroll( GLFWwindow* window, double xoffset, double yoffset ) {
    if ( !ImGui::GetCurrentContext() )
        return false;

    ImGui_ImplGlfw_ScrollCallback( window, xoffset, yoffset );
    return ImGui::GetIO().WantCaptureMouse;
}

void GLFWWindow::onGUIWindowFocus( GLFWwindow* window, int focused ) {
    if ( ImGui::GetCurrentContext() )
        ImGui_ImplGlfw_WindowFocusCallback( window, focused );
}

void GLFWWindow::onGUICursorEnter( GLFWwindow* window, int entered ) {
    if ( ImGui::GetCurrentContext() )
        ImGui_ImplGlfw_CursorEnterCallback( window, entered );
}

} // namespace Graphics

AXION_NAMESPACE_END