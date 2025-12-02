#pragma once
#include "Axion/Graphics/Platforms/Win32.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class Win32Window final : public IWin32Window
{
public:
    Win32Window( HINSTANCE hInstance, const Settings& settings = {} );
    ~Win32Window() override;
    Win32Window( const Win32Window& )            = delete;
    Win32Window& operator=( const Win32Window& ) = delete;

    bool processMessages() override;

    void setFullscreen( bool fullscreen ) override;
    bool minimized() const override;

    const Settings& getSettings() const override { return _settings; }
    void            setSettings( const Settings& settings ) override { _settings = settings; }

    bool              shouldClose() const override { return _shouldClose; }
    PlatformType      getPlatformType() const override { return PlatformType::Win32; }
    RHI::NativeObject getNativeObject() override;

    Event::EventDispatcher<Event::WindowResizeEvent>& onResize() override { return _onResize; }
    Event::EventDispatcher<Event::WindowCloseEvent>&  onClose() override { return _onClose; }
    Event::EventDispatcher<Event::KeyEvent>&          onKey() override { return _onKey; }
    Event::EventDispatcher<Event::MouseButtonEvent>&  onMouseButton() override { return _onMouseButton; }
    Event::EventDispatcher<Event::MouseMoveEvent>&    onMouseMove() override { return _onMouseMove; }
    Event::EventDispatcher<Event::MouseScrollEvent>&  onMouseScroll() override { return _onMouseScroll; }

private:
    static LRESULT CALLBACK wndProcSetup( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
    static LRESULT CALLBACK wndProcThunk( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
    LRESULT CALLBACK        wndProcMsg( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

    constexpr static Event::KeyCode mapWin32Key( WPARAM wParam ) {
        switch ( wParam ) {
            case 'W':       return Event::KeyCode::W;
            case 'A':       return Event::KeyCode::A;
            case 'S':       return Event::KeyCode::S;
            case 'D':       return Event::KeyCode::D;
            case 'Q':       return Event::KeyCode::Q;
            case 'E':       return Event::KeyCode::E;

            case VK_UP:     return Event::KeyCode::Up;
            case VK_DOWN:   return Event::KeyCode::Down;
            case VK_LEFT:   return Event::KeyCode::Left;
            case VK_RIGHT:  return Event::KeyCode::Right;

            case VK_SPACE:  return Event::KeyCode::Space;
            case VK_ESCAPE: return Event::KeyCode::Escape;
            case VK_SHIFT:  return Event::KeyCode::Shift;

            default:        return Event::KeyCode::Unknown;
        }
    }

    HWND      _hWnd         = nullptr; // Native type handle
    HINSTANCE _hInstance    = nullptr;
    LPCWSTR   _wndClassName = L"Win32Window";
    RECT      _rect         = {};

    Settings _settings    = {};
    bool     _initialized = false;
    bool     _shouldClose = false;
    bool     _minimized   = false;

    // Event Managing
    Event::EventDispatcher<Event::WindowResizeEvent> _onResize;
    Event::EventDispatcher<Event::WindowCloseEvent>  _onClose;
    Event::EventDispatcher<Event::KeyEvent>          _onKey;
    Event::EventDispatcher<Event::MouseButtonEvent>  _onMouseButton;
    Event::EventDispatcher<Event::MouseMoveEvent>    _onMouseMove;
    Event::EventDispatcher<Event::MouseScrollEvent>  _onMouseScroll;
    // Event::EventDispatcher<Event::WindowFocusEvent>  _onFocus;
};

} // namespace Graphics
AXION_NAMESPACE_END
