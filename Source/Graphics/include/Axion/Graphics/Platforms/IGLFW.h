#pragma once
#include <Axion/Graphics/Platforms/IWindow.h>
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>


AXION_NAMESPACE_BEGIN

namespace Graphics {

/**
 * @brief Handle definition for GLFW-specific window implementation.
 */
DEFINE_OWNER_PTR_FOR_TYPE( IGLFWWindow, GLFWWindow )

/**
 * @brief Interface for a GLFW window implementation.
 *
 * This interface represents a platform window using the GLFW library.
 * It is primarily used for cross-platform window creation (Win32, Linux, macOS)
 * and provides Vulkan-compatible surface creation, although it is also compatible with DirectX12.
 *
 * @see GLFWWindow
 * @see IWindow
 */
class IGLFWWindow : public IWindow
{
protected:
};

/**
 * @brief Factory function to create a GLFW-based window.
 *
 * Creates and initializes a window using the GLFW toolkit, configured
 * according to the provided @ref WindowSettings object. This function
 * abstracts platform differences and is typically used when the engine
 * is running with multi-platform support.
 *
 * @param settings Window configuration (name, size, fullscreen, etc.).
 *
 * @return A handle to the created window object.
 *
 *
 * @see GLFWWindow
 * @see IWindow
 * @see createWindowForWin32()
 */
WindowOwnerPtr createWindowForGLFW( const WindowSettings& settings = {} );

} // namespace Graphics

AXION_NAMESPACE_END
