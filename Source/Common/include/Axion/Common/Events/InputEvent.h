#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Common/Events/Event.h"

AXION_NAMESPACE_BEGIN

namespace Event {
 
enum class KeyCode : u32
{
    Unknown = 0,
    
    A, B, C, D, E, F, G, H, I, J, K, L, M, 
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Up, Down, Left, Right,

    F1, F2, F3, F4,
    Space, Escape, Enter, Shift, Control, Alt, Tab,

    // ... 
};
    
/**
 * @brief Base input event type.
 */
struct InputEvent : public Event {
    explicit InputEvent( void* h )
        : handle( h ) {}
    void* handle; ///< Native window handle (HWND on Win32)
};

/**
 * @brief Keyboard event base.
 */
struct KeyEvent : public InputEvent {
    KeyEvent( void* h, KeyCode k, bool pressed )
        : InputEvent( h )
        , keyCode( k )
        , pressed( pressed ) {}
    KeyCode keyCode;
    bool pressed;
};

/**
 * @brief Mouse button press/release event.
 */
struct MouseButtonEvent : public InputEvent {
    MouseButtonEvent( void* h, u32 button, bool pressed )
        : InputEvent( h )
        , button( button )
        , pressed( pressed ) {}
    u32 button;
    bool pressed;
};

/**
 * @brief Mouse move event.
 */
struct MouseMoveEvent : public InputEvent {
    MouseMoveEvent( void* h, int32_t x, int32_t y )
        : InputEvent( h )
        , x( x )
        , y( y ) {}
    int32_t x;
    int32_t y;
};

/**
 * @brief Mouse wheel scroll event.
 */
struct MouseScrollEvent : public InputEvent {
    MouseScrollEvent( void* h, float delta )
        : InputEvent( h )
        , delta( delta ) {}
    float delta;
};

} // namespace Event

AXION_NAMESPACE_END
