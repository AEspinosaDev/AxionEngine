#pragma once
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>
#include "Axion/Common/Common.h"
#include "Axion/Common/Events/InputEvent.h"
#include "Axion/Common/Events/WindowEvent.h"
#include "Axion/Graphics/RHI/IDevice.h"



AXION_NAMESPACE_BEGIN

namespace Graphics {

DEFINE_OWNER_PTR_FOR_TYPE( IWindow, Window )

/**
 * @brief Interface for all window types across platforms.
 *
 * Provides a common abstraction for platform-specific window systems
 * such as Win32, GLFW, or SDL. Responsible for event processing,
 * fullscreen management, and native handle interop.
 */
class IWindow
{
public:
    /**
     * @brief Holds configuration and state for a window.
     */
    struct Settings {
        std::string name              = "Axion Window"; //!< Window title
        Extent2D    size              = { 1280, 720 };  //!< Initial window size
        bool        fullscreen        = false;          //!< Fullscreen mode flag
        bool        centered          = true;           //!< Whether to center the window
        Position2D  position          = { 100, 100 };   //!< Position on the screen (if not centered)
        std::string iconPath          = "";             //!< Path to window icon
        std::string cursorPath        = "";             //!< Path to cursor icon
        int         style             = 0;              //!< Platform-specific style flags
        bool        enableGuiInputCBs = true;           //!< Enable Gui Input
    };

    /** @brief Virtual destructor for safe polymorphic deletion. */
    virtual ~IWindow() = default;

    /**
     * @brief Processes pending OS messages (input, resize, close, etc.).
     *
     * @return True if messages were processed successfully.
     */
    virtual bool processMessages() = 0;

    /**
     * @brief Enables or disables fullscreen mode.
     *
     * @param fullscreen Set to true to enable fullscreen.
     */
    virtual void setFullscreen( bool fullscreen ) = 0;

    /**
     * @brief Checks whether the window should close.
     *
     * Typically triggered by the user clicking the close button or
     * receiving a system close event.
     *
     * @return True if the window should close, false otherwise.
     */
    virtual bool shouldClose() const = 0;

    /**
     * @brief Checks whether the window is minimized.
     *
     * Typically triggered by the user clicking the minimize button.
     *
     * @return True if the window is minimized, false otherwise.
     */
    virtual bool minimized() const = 0;

    /**
     * @brief Returns the current window configuration.
     * @return Constant reference to the window settings.
     */
    virtual const Settings& getSettings() const = 0;

    virtual void setTitle( const std::string& title ) = 0;

    /**
     * @brief Updates the window configuration.
     *
     * @param settings The new window settings to apply.
     */
    virtual void setSettings( const Settings& settings ) = 0;

    /**
     * @brief Returns the platform backend type.
     *
     * For example: `PlatformType::Win32`, `PlatformType::GLFW`, or `PlatformType::SDL`.
     *
     * @return The platform type.
     */
    virtual PlatformType getPlatformType() const = 0;

    /**
     * @brief Creates a native handle suitable for interop with the RHI.
     *
     * @return A populated RHI::NativeObject structure.
     */
    virtual RHI::NativeObject getNativeObject() = 0;

    // Event handling
    virtual Event::EventDispatcher<Event::WindowResizeEvent>& onResize()      = 0;
    virtual Event::EventDispatcher<Event::WindowCloseEvent>&  onClose()       = 0;
    virtual Event::EventDispatcher<Event::KeyEvent>&          onKey()         = 0;
    virtual Event::EventDispatcher<Event::MouseButtonEvent>&  onMouseButton() = 0;
    virtual Event::EventDispatcher<Event::MouseMoveEvent>&    onMouseMove()   = 0;
    virtual Event::EventDispatcher<Event::MouseScrollEvent>&  onMouseScroll() = 0;
};

/// Alias for convenience when referring to window settings.
typedef IWindow::Settings WindowSettings;

} // namespace Graphics

AXION_NAMESPACE_END
