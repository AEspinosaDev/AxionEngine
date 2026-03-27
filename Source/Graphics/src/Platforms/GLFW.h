#pragma once
#include "Axion/Graphics/Platforms/IGLFW.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

class GLFWWindow final : public IGLFWWindow
{
public:
    GLFWWindow( const Settings& settings = {} );
    ~GLFWWindow() override;
    GLFWWindow( const GLFWWindow& )            = delete;
    GLFWWindow& operator=( const GLFWWindow& ) = delete;

    bool processMessages() override;

    void setFullscreen( bool fullscreen ) override;
    bool minimized() const override;

    const Settings& getSettings() const override { return _setts; }
    void            setSettings( const Settings& settings ) override { _setts = settings; }
    void            setTitle( const std::string& title ) override;

    bool              shouldClose() const override { return _shouldClose; }
    PlatformType      getPlatformType() const override { return PlatformType::GLFW; }
    RHI::NativeObject getNativeObject() override;

    Event::EventDispatcher<Event::WindowResizeEvent>& onResize() override { return _onResize; }
    Event::EventDispatcher<Event::WindowCloseEvent>&  onClose() override { return _onClose; }
    Event::EventDispatcher<Event::KeyEvent>&          onKey() override { return _onKey; }
    Event::EventDispatcher<Event::MouseButtonEvent>&  onMouseButton() override { return _onMouseButton; }
    Event::EventDispatcher<Event::MouseMoveEvent>&    onMouseMove() override { return _onMouseMove; }
    Event::EventDispatcher<Event::MouseScrollEvent>&  onMouseScroll() override { return _onMouseScroll; }

private:
    void setCallbacksFunctions();

    bool onGUIKey( GLFWwindow* window, int key, int scancode, int action, int mods );
    bool onGUIChar( GLFWwindow* window, unsigned int c );
    bool onGUIMouseButton( GLFWwindow* window, int button, int action, int mods );
    bool onGUIMouseMove( GLFWwindow* window, double x, double y );
    bool onGUIMouseScroll( GLFWwindow* window, double xoffset, double yoffset );
    void onGUIWindowFocus( GLFWwindow* window, int focused );
    void onGUICursorEnter( GLFWwindow* window, int entered );

    //clang-format off
    constexpr static Event::KeyCode mapGLFWKey( int glfwKey ) {
        switch ( glfwKey )
        {
            case GLFW_KEY_A:
                return Event::KeyCode::A;
            case GLFW_KEY_B:
                return Event::KeyCode::B;
            case GLFW_KEY_C:
                return Event::KeyCode::C;
            case GLFW_KEY_D:
                return Event::KeyCode::D;
            case GLFW_KEY_E:
                return Event::KeyCode::E;
            case GLFW_KEY_F:
                return Event::KeyCode::F;
            case GLFW_KEY_G:
                return Event::KeyCode::G;
            case GLFW_KEY_H:
                return Event::KeyCode::H;
            case GLFW_KEY_I:
                return Event::KeyCode::I;
            case GLFW_KEY_J:
                return Event::KeyCode::J;
            case GLFW_KEY_K:
                return Event::KeyCode::K;
            case GLFW_KEY_L:
                return Event::KeyCode::L;
            case GLFW_KEY_M:
                return Event::KeyCode::M;
            case GLFW_KEY_N:
                return Event::KeyCode::N;
            case GLFW_KEY_O:
                return Event::KeyCode::O;
            case GLFW_KEY_P:
                return Event::KeyCode::P;
            case GLFW_KEY_Q:
                return Event::KeyCode::Q;
            case GLFW_KEY_R:
                return Event::KeyCode::R;
            case GLFW_KEY_S:
                return Event::KeyCode::S;
            case GLFW_KEY_T:
                return Event::KeyCode::T;
            case GLFW_KEY_U:
                return Event::KeyCode::U;
            case GLFW_KEY_V:
                return Event::KeyCode::V;
            case GLFW_KEY_W:
                return Event::KeyCode::W;
            case GLFW_KEY_X:
                return Event::KeyCode::X;
            case GLFW_KEY_Y:
                return Event::KeyCode::Y;
            case GLFW_KEY_Z:
                return Event::KeyCode::Z;

            case GLFW_KEY_UP:
                return Event::KeyCode::Up;
            case GLFW_KEY_DOWN:
                return Event::KeyCode::Down;
            case GLFW_KEY_LEFT:
                return Event::KeyCode::Left;
            case GLFW_KEY_RIGHT:
                return Event::KeyCode::Right;

            case GLFW_KEY_SPACE:
                return Event::KeyCode::Space;
            case GLFW_KEY_ESCAPE:
                return Event::KeyCode::Escape;
            case GLFW_KEY_ENTER:
                return Event::KeyCode::Enter;
            case GLFW_KEY_TAB:
                return Event::KeyCode::Tab;
                // case GLFW_KEY_BACKSPACE: return Event::KeyCode::Backspace;

            case GLFW_KEY_LEFT_SHIFT:
                return Event::KeyCode::Shift;
            case GLFW_KEY_RIGHT_SHIFT:
                return Event::KeyCode::Shift;
            case GLFW_KEY_LEFT_CONTROL:
                return Event::KeyCode::Control;
            case GLFW_KEY_RIGHT_CONTROL:
                return Event::KeyCode::Control;
            case GLFW_KEY_LEFT_ALT:
                return Event::KeyCode::Alt;

                // // --- Números (Top Row) ---
                // case GLFW_KEY_0: return Event::KeyCode::Num0;
                // case GLFW_KEY_1: return Event::KeyCode::Num1;
                // // ... hasta 9

            default:
                return Event::KeyCode::Unknown;
        }
    }
    //clang-format on
    GLFWwindow* _hWnd = nullptr; // Native type handle

    Settings _setts       = {};
    bool     _initialized = false;
    bool     _shouldClose = false;
    bool     _minimized   = false;

    Extent2D _windowedSizeCache;

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
