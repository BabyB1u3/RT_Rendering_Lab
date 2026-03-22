#pragma once

/// @file Input.h
/// @brief Polling-based input system (keyboard + mouse) backed by GLFW.
///
/// Design:  Stateless polling — every call queries GLFW for the current key/button
///          state, so there is no event queue or buffering.
///
/// Mouse delta tracking:
///   GetMouseDelta() computes (currentPos - lastPos) each time it is called.
///   The very first call after Initialize() always returns (0, 0) to avoid a
///   large jump from the default (0, 0) origin to the actual cursor position.
///
/// Thread safety: All methods must be called from the main (GLFW) thread.
///
/// See also: KeyCode.h / MouseCode.h for typed enum constants.

#include <utility>

struct GLFWwindow;

class Input
{
public:
    /// Bind to a GLFW window. Must be called once before any query method.
    static void Initialize(GLFWwindow *window);

    /// True if the key is currently held down (GLFW_PRESS or GLFW_REPEAT).
    /// @param key  A GLFW key code or a Key::Code value from KeyCode.h.
    static bool IsKeyPressed(int key);
    /// True if the mouse button is currently pressed.
    /// @param button  A GLFW button code or a Mouse::Code value from MouseCode.h.
    static bool IsMouseButtonPressed(int button);

    /// Current cursor position in window-space pixels.
    static std::pair<float, float> GetMousePosition();
    /// Cursor movement since the last call to GetMouseDelta().
    /// Returns (0, 0) on the first call to prevent a large initial jump.
    static std::pair<float, float> GetMouseDelta();

    static float GetMouseX();
    static float GetMouseY();

private:
    static GLFWwindow *s_Window;   // Non-owning. Lifetime managed by Window.
    static float s_LastMouseX;
    static float s_LastMouseY;
    static bool s_FirstMouseSample; // Guards against the first-frame jump.
};
