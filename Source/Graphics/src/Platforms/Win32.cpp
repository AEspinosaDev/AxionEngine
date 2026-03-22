#include "Win32.hpp"
#include "Win32ImGuiSetup.h"


AXION_NAMESPACE_BEGIN

namespace Graphics {

WindowPtr createWindowForWin32( HINSTANCE hInstance, const WindowSettings& settings ) {
    return NEW_U( Win32Window )( hInstance, settings );
}

Win32Window::Win32Window( HINSTANCE hInstance, const Settings& settings )
    : _hInstance( hInstance )
    , _settings( settings ) {

    // Window class registration
    //-------------------------------------

    WNDCLASSEXW windowClass = {};

    windowClass.cbSize        = sizeof( WNDCLASSEX );
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = &wndProcSetup;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = hInstance;
    windowClass.hIcon         = ::LoadIcon( hInstance, NULL );
    windowClass.hCursor       = ::LoadCursor( NULL, IDC_ARROW );
    windowClass.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = _wndClassName;
    windowClass.hIconSm       = ::LoadIcon( hInstance, NULL );

    static ATOM atom = ::RegisterClassExW( &windowClass );
    assert( atom > 0 );

    int windowX = (int)_settings.position.x;
    int windowY = (int)_settings.position.y;

    // Window initiation
    //-------------------------------------

    if ( _settings.centered )
    {
        int screenWidth  = ::GetSystemMetrics( SM_CXSCREEN );
        int screenHeight = ::GetSystemMetrics( SM_CYSCREEN );

        RECT windowRect = { 0, 0, static_cast<LONG>( _settings.size.width ), static_cast<LONG>( _settings.size.height ) };
        ::AdjustWindowRect( &windowRect, WS_OVERLAPPEDWINDOW, FALSE );

        int windowWidth  = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
        windowX            = std::max<int>( 0, ( screenWidth - windowWidth ) / 2 );
        windowY            = std::max<int>( 0, ( screenHeight - windowHeight ) / 2 );
        _settings.position = { (uint)windowX, (uint)windowY };
    }

    _hWnd = CreateWindowExW(
        0,
        _wndClassName,
        std::wstring( _settings.name.begin(), _settings.name.end() ).c_str(),
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        _settings.size.width,
        _settings.size.height,
        nullptr,
        nullptr,
        _hInstance,
        this );

    ::GetWindowRect( _hWnd, &_rect );

    AXION_LOG_ASSERT( _hWnd, Logger::Module::GFX, "[FATAL] Failed to create Win32 window" );

    _initialized = true;

    AXION_LOG_INFO( Logger::Module::GFX, "Window for Platform Win32 Created Successfully" );
    AXION_LOG_INFO( Logger::Module::GFX, "Window Size: \n Width = {} \n Height = {} \n", _settings.size.width, _settings.size.height );

    ShowWindow( _hWnd, SW_SHOW );
}

Win32Window::~Win32Window() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Platform Win32 Window" );
    ::DestroyWindow( _hWnd );
}
bool Win32Window::processMessages() {
    MSG msg = {};
    while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );

        if ( msg.message == WM_QUIT )
            _shouldClose = true;
    }
    return !_shouldClose;
}

void Win32Window::setFullscreen( bool fullscreen ) {
    if ( _settings.fullscreen != fullscreen )
    {
        _settings.fullscreen = fullscreen;

        if ( _settings.fullscreen ) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored
            // when switching out of fullscreen state.
            ::GetWindowRect( _hWnd, &_rect );

            // Set the window style to a borderless window so the client area fills
            // the entire screen.
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~( WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX );

            ::SetWindowLongW( _hWnd, GWL_STYLE, windowStyle );

            // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR      hMonitor    = ::MonitorFromWindow( _hWnd, MONITOR_DEFAULTTONEAREST );
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize        = sizeof( MONITORINFOEX );
            ::GetMonitorInfo( hMonitor, &monitorInfo );

            ::SetWindowPos( _hWnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE );

            ::ShowWindow( _hWnd, SW_MAXIMIZE );

        } else
        {
            // Restore all the window decorators.
            ::SetWindowLong( _hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW );

            ::SetWindowPos( _hWnd, HWND_NOTOPMOST, _rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE );

            ::ShowWindow( _hWnd, SW_NORMAL );
        }
    }
}
bool Win32Window::minimized() const {
    return _minimized;
}
void Win32Window::setTitle( const std::string& title ) {
    ::SetWindowTextA( _hWnd, title.c_str() );
}

RHI::NativeObject Win32Window::getNativeObject() {
    auto native = RHI::NativeObject( RHI::ObjectTypes::WIN32_WINDOW, _hWnd );
    return native;
}
LRESULT Win32Window::wndProcSetup( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    if ( msg == WM_NCCREATE )
    {
        CREATESTRUCT* cs     = reinterpret_cast<CREATESTRUCT*>( lParam );
        Win32Window*  window = reinterpret_cast<Win32Window*>( cs->lpCreateParams );
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)window );
        SetWindowLongPtr( hwnd, GWLP_WNDPROC, (LONG_PTR)wndProcThunk );
        return window->wndProcMsg( hwnd, msg, wParam, lParam );
    }
    return DefWindowProc( hwnd, msg, wParam, lParam );
}
LRESULT Win32Window::wndProcThunk( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    Win32Window* window = reinterpret_cast<Win32Window*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
    return window->wndProcMsg( hwnd, msg, wParam, lParam );
}
LRESULT Win32Window::wndProcMsg( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

    bool guiWantsKb    = false;
    bool guiWantsMouse = false;
    if ( _settings.enableGuiInputCBs )
        wndProcGUI( hwnd, msg, wParam, lParam, guiWantsKb, guiWantsMouse );

    switch ( msg )
    {
        // --- Keyboard ---
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if ( guiWantsKb )
                return 0;
            Event::KeyEvent evt( hwnd, mapWin32Key( wParam ), true );
            if ( wParam == VK_F11 )
                setFullscreen( !_settings.fullscreen );
            _onKey.dispatch( evt );
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if ( guiWantsKb )
                return 0;
            Event::KeyEvent evt( hwnd, mapWin32Key( wParam ), false );
            _onKey.dispatch( evt );
            return 0;
        }

        // --- Mouse buttons ---
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if ( guiWantsMouse )
                return 0;
            uint                    btn = ( msg == WM_LBUTTONDOWN ? 0 : msg == WM_RBUTTONDOWN ? 1
                                                                                              : 2 );
            Event::MouseButtonEvent evt( hwnd, btn, true );
            _onMouseButton.dispatch( evt );
            return 0;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if ( guiWantsMouse )
                return 0;
            uint                    btn = ( msg == WM_LBUTTONUP ? 0 : msg == WM_RBUTTONUP ? 1
                                                                                          : 2 );
            Event::MouseButtonEvent evt( hwnd, btn, false );
            _onMouseButton.dispatch( evt );
            return 0;
        }

        // --- Mouse movement ---
        case WM_MOUSEMOVE: {
            if ( guiWantsMouse )
                return 0;
            // int32_t               x = GET_X_LPARAM( lParam );
            // int32_t               y = GET_Y_LPARAM( lParam );
            int32_t               x = GET_XBUTTON_WPARAM( lParam );
            int32_t               y = GET_XBUTTON_WPARAM( lParam );
            Event::MouseMoveEvent evt( hwnd, x, y );
            _onMouseMove.dispatch( evt );
            return 0;
        }

        // --- Mouse wheel ---
        case WM_MOUSEWHEEL: {
            if ( guiWantsMouse )
                return 0;
            float                   delta = GET_WHEEL_DELTA_WPARAM( wParam ) / (float)WHEEL_DELTA;
            Event::MouseScrollEvent evt( hwnd, delta );
            _onMouseScroll.dispatch( evt );
            return 0;
        }
        case WM_SIZE: {
            uint width     = LOWORD( lParam );
            uint height    = HIWORD( lParam );
            _minimized     = width == 0 && height == 0 ? true : false;
            _settings.size = { width, height };
            Event::WindowResizeEvent evt( hwnd, width, height );
            _onResize.dispatch( evt );
            return 0;
        }

        case WM_CLOSE: {
            _shouldClose = true;
            Event::WindowCloseEvent evt( hwnd );
            _onClose.dispatch( evt );
            PostQuitMessage( 0 );
            return 0;
        }
        case WM_DESTROY: {
            Event::WindowCloseEvent evt( hwnd );
            _onClose.dispatch( evt );
            ::PostQuitMessage( 0 );
        }
        default:
            return ::DefWindowProcW( hwnd, msg, wParam, lParam );

            // case WM_SETFOCUS: {
            //      Event::WindowFocusEvent evt( hwnd, true );
            //     _onFocus.dispatch( evt );
            //     return 0;
            // }

            // case WM_KILLFOCUS: {
            //      Event::WindowFocusEvent evt( hwnd, false );
            //     _onFocus.dispatch( evt );
            //     return 0;
            // }

            // (other window events as before...)
    }
}

void Win32Window::wndProcGUI( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& guiWantsKb, bool& guiWantsMouse ) {

    // 1. Let ImGui handle the message first
    if ( ImGui::GetCurrentContext() != nullptr )
    {
        if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wParam, lParam ) )
        {
            return;
        }
    }

    if ( ImGui::GetCurrentContext() != nullptr )
    {
        ImGuiIO& io   = ImGui::GetIO();
        guiWantsKb    = io.WantCaptureKeyboard;
        guiWantsMouse = io.WantCaptureMouse;
    }
}

} // namespace Graphics

// namespace Renderer
AXION_NAMESPACE_END