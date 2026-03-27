#pragma once

/// @file Input.h
/// @brief Polling-based input system with double-buffered state and edge detection.
///
/// Design: Double-buffered per-frame snapshots of GLFW keyboard/mouse state.
///   BeginFrame() must be called once at the top of the frame loop (before any
///   layer's OnUpdate). This enables:
///     - IsKeyDown / IsMouseButtonDown   - held right now
///     - WasKeyPressedThisFrame          - down this frame, up last frame
///     - WasKeyReleasedThisFrame         - up this frame, down last frame
///
/// Scroll is the one exception to polling: GLFW only reports it via callback,
/// so we accumulate it and expose it per-frame via GetScrollDelta().
///
/// Capture flags (set by ImGuiLayer) block polling so game/demo code doesn't
/// respond to input that ImGui is consuming.
///
/// Thread safety: All methods must be called from the main (GLFW) thread.
///
/// See also: KeyCode.h / MouseCode.h for typed enum constants.

#include <utility>
#include "core/input/KeyCode.h"
#include "core/input/MouseCode.h"

struct GLFWwindow;

class Input
{
public:
    // --- Lifecycle (called by Application, not by user code) ---

    /// Bind to a GLFW window. Must be called once before any query method.
    static void Initialize(GLFWwindow *window);

    /// Must be called ONCE per frame, BEFORE any layer's OnUpdate().
    /// Snapshots current GLFW state into the "current" buffer and
    /// moves the old "current" into "previous".
    static void BeginFrame();

    // --- Keyboard ---

    /// True if the key is currently held down.
    static bool IsKeyDown(Key::Code key);
    /// True only on the frame the key transitioned from up to down.
    static bool WasKeyPressedThisFrame(Key::Code key);
    /// True only on the frame the key transitioned from down to up.
    static bool WasKeyReleasedThisFrame(Key::Code key);

    // --- Mouse buttons ---

    /// True if the mouse button is currently pressed.
    static bool IsMouseButtonDown(Mouse::Code button);
    /// True only on the frame the button transitioned from up to down.
    static bool WasMouseButtonPressedThisFrame(Mouse::Code button);
    /// True only on the frame the button transitioned from down to up.
    static bool WasMouseButtonReleasedThisFrame(Mouse::Code button);

    // --- Mouse position & delta ---

    /// Current cursor position in window-space pixels.
    static std::pair<float, float> GetMousePosition();
    /// Cursor movement since the previous frame.
    static std::pair<float, float> GetMouseDelta();
    static float GetMouseX();
    static float GetMouseY();

    // --- Mouse scroll (accumulated this frame) ---

    /// Returns the scroll Y delta accumulated since the last BeginFrame().
    static float GetScrollDelta();

    /// Fed by the GLFW scroll callback. Not for user code.
    static void AccumulateScroll(float yOffset);

    // --- Capture flags (set by ImGui layer) ---

    static void SetKeyboardCaptured(bool captured);
    static void SetMouseCaptured(bool captured);
    static bool IsKeyboardCaptured();
    static bool IsMouseCaptured();

    // --- Backwards-compatible aliases ---

    /// @deprecated Use IsKeyDown() instead.
    static bool IsKeyPressed(Key::Code key) { return IsKeyDown(key); }
    /// @deprecated Use IsMouseButtonDown() instead.
    static bool IsMouseButtonPressed(Mouse::Code button) { return IsMouseButtonDown(button); }

private:
    static GLFWwindow *s_Window;

    // Double-buffered keyboard state. GLFW_KEY_LAST = 348, 512 is safe.
    static constexpr int KEY_STATE_SIZE = 512;
    static bool s_CurrentKeys[KEY_STATE_SIZE];
    static bool s_PreviousKeys[KEY_STATE_SIZE];

    // Double-buffered mouse button state.
    static constexpr int MOUSE_BUTTON_COUNT = 8;
    static bool s_CurrentMouseButtons[MOUSE_BUTTON_COUNT];
    static bool s_PreviousMouseButtons[MOUSE_BUTTON_COUNT];

    // Mouse position tracking.
    static float s_MouseX, s_MouseY;
    static float s_LastMouseX, s_LastMouseY;
    static bool s_FirstMouseSample;

    // Scroll accumulator (set via GLFW scroll callback, consumed per frame).
    static float s_ScrollAccumulator;
    static float s_ScrollThisFrame;

    // Capture flags.
    static bool s_KeyboardCaptured;
    static bool s_MouseCaptured;
};
