#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Events/InputEvent.h>
#include <Axion/Common/Events/WindowEvent.h>
#include <Axion/Common/Graphics/Defines.h>

AXION_NAMESPACE_BEGIN

// Forward declare
namespace Graphics {
class IWindow;
}

namespace Core::Platform {

/**
 * @brief High-level abstraction for an Operating System Window.
 *
 * This class acts as the main entry point for window management in the Core module.
 * It employs the PIMPL (Pointer to Implementation) idiom to hide platform-specific
 * dependencies (like Win32 HWNDs or GLFW pointers) from the public header.
 *
 * @note This class owns the lifecycle of the underlying `Graphics::IWindow`.
 */
class Window
{
public:
    /**
     * @brief Configuration structure for window creation.
     */
    struct Settings {

        Graphics::PlatformType platformType = Graphics::PlatformType::Win32;
        bool                   vsync        = false;
        std::string            name         = "Axion Window";
        Extent2D               size         = { 1280, 720 };
        bool                   fullscreen   = false;
        bool                   centered     = true;
        Position2D             position     = { 100, 100 };
        std::string            iconPath     = "";
        std::string            cursorPath   = "";
        int                    style        = 0;

    };

    /**
     * @brief Creates a new Window instance.
     *
     * Initializes the PIMPL implementation and creates the native OS window immediately.
     *
     * @param settings Configuration parameters for the window.
     */
    Window( const Settings& settings = {} );

    /**
     * @brief Destroys the window and releases native OS resources.
     */
    ~Window();

    Window( const Window& )            = delete;
    Window& operator=( const Window& ) = delete;

    /**
     * @brief Processes pending OS messages (polling).
     *
     * This method should be called once per frame in the main loop.
     * It handles input events, resizing, and window movements.
     *
     * @return True if the window is valid and running, False if a close request was received.
     */
    bool update();

    /**
     * @brief Toggles the fullscreen state of the window.
     * @param fullscreen True for fullscreen, false for windowed mode.
     */
    void setFullscreen( bool fullscreen );

    /**
     * @brief Checks if the window has received a request to close (e.g., user pressed 'X').
     * @return True if the window should close.
     */
    bool shouldClose() const;

    /**
     * @brief Gets the current dimensions of the window's client area.
     * @return The width and height in pixels.
     */
    Extent2D getSize() const;

    /**
     * @brief Resizes the window.
     * @param size The new width and height in pixels.
     */
    void setSize( const Extent2D& size );

    /**
     * @brief Checks if the window is currently minimized to the taskbar.
     * Useful to pause the rendering loop and save resources.
     * @return True if minimized.
     */
    bool minimized() const;

    /**
     * @brief Retrieves the settings used to configure this window.
     * @return Constant reference to the settings structure.
     */
    const Settings& getSettings() const;

    /**
     * @brief Gets the platform backend type currently in use.
     * @return The active platform type (e.g., Win32, GLFW).
     */
    Graphics::PlatformType getPlatformType() const;

    // ------------------------------------------------------------------------
    // Low Level Interop
    // ------------------------------------------------------------------------

    /**
     * @brief Accessor for the underlying low-level window interface.
     *
     * Provides access to the `Graphics::IWindow` interface. This is required
     * by the `Graphics::IRenderer` to create the SwapChain and Surface.
     *
     * @warning The returned pointer is owned by this `Core::Window` instance.
     * Do not delete this pointer manually.
     *
     * @return Raw pointer to the internal graphics window implementation.
     */
    Graphics::IWindow* getNativeWindow() const;

    // ------------------------------------------------------------------------
    // Event Handling
    // ------------------------------------------------------------------------

    /** @brief Dispatcher for window resize events. */
    Event::EventDispatcher<Event::WindowResizeEvent>& onResize();

    /** @brief Dispatcher for window close requests. */
    Event::EventDispatcher<Event::WindowCloseEvent>& onClose();

    /** @brief Dispatcher for keyboard press/release events. */
    Event::EventDispatcher<Event::KeyEvent>& onKey();

    /** @brief Dispatcher for mouse button clicks. */
    Event::EventDispatcher<Event::MouseButtonEvent>& onMouseButton();

    /** @brief Dispatcher for mouse movement events. */
    Event::EventDispatcher<Event::MouseMoveEvent>& onMouseMove();

    /** @brief Dispatcher for mouse wheel scrolling. */
    Event::EventDispatcher<Event::MouseScrollEvent>& onMouseScroll();


    std::string toString() const;


private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace Core::Platform

AXION_NAMESPACE_END