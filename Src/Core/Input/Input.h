#pragma once

/// @file Input.h
/// @brief Polling-based input facade over the device-based input layer.
///
/// Design: Double-buffered per-frame snapshots of keyboard/mouse/gamepad state.
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
/// See also: KeyCode.h / MouseCode.h / GamepadCode.h for typed constants.

#include <array>
#include <utility>

#include "Core/Util/Base.h"
#include "Core/Input/Device/InputDevice.h"
#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Device/KeyboardDevice.h"
#include "Core/Input/Code/KeyCode.h"
#include "Core/Input/Code/MouseCode.h"
#include "Core/Input/Device/MouseDevice.h"

class EventBus;
class GamepadDevice;
class InputDeviceManager;
struct GLFWwindow;

namespace TestSupport
{
struct InputTestAccess;
}

class Input
{
public:
    static constexpr int k_KeyStateSize = KeyboardDevice::k_KeyStateSize;
    static constexpr int k_MouseButtonCount = MouseDevice::k_ButtonCount;
    static constexpr int k_MaxGamepadCount = 4;

    struct PolledState
    {
        std::array<bool, k_KeyStateSize> keys{};
        std::array<bool, k_MouseButtonCount> mouseButtons{};
        float mouseX = 0.0f;
        float mouseY = 0.0f;
    };

    struct GamepadPolledState
    {
        bool isConnected = false;
        std::array<bool, GamepadButton::Count> buttons{};
        std::array<float, GamepadAxis::Count> axes{};
    };

public:
    // --- Lifecycle (called by Application, not by user code) ---

    /// Bind to a GLFW window. Repeating Initialize() with the same non-null
    /// window is a no-op so layer re-attach does not wipe live state.
    static void Initialize(GLFWwindow* window);
    /// Inject the application event bus so device hot-plug notifications can
    /// be published without introducing global state.
    static void SetEventBus(EventBus* bus);
    /// Must be called ONCE per frame, BEFORE any layer's OnUpdate().
    /// Polls all registered devices and advances their current/previous state.
    static void BeginFrame(float dt = 0.0f);
    /// Register or replace a device in its logical type/index slot.
    static void RegisterDevice(Scope<InputDevice> device);
    /// Restore the default live device layout exactly (keyboard, mouse, four gamepads).
    /// Any extra custom or non-default-slot devices are removed.
    static void RestoreDefaultDevices();

    static InputDeviceManager& GetDeviceManager();
    /// Non-owning pointer. Returns null until the device manager has been created.
    static const InputDeviceManager* TryGetDeviceManager();

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
    /// Not blocked by SetMouseCaptured() - absolute cursor position is always
    /// readable regardless of capture state. For input-action queries that
    /// respect capture, use GetMouseDelta(), IsMouseButtonDown(), etc.
    static std::pair<float, float> GetMousePosition();
    /// Cursor movement since the previous frame.
    static std::pair<float, float> GetMouseDelta();
    static float GetMouseX();
    static float GetMouseY();
    /// Returns the scroll Y delta accumulated since the last BeginFrame().
    static float GetScrollDelta();
    /// Fed by the GLFW scroll callback. Not for user code.
    static void AccumulateScroll(float yOffset);

    // --- Gamepad convenience facade ---
    /// True if the logical gamepad slot is currently connected.
    static bool IsGamepadConnected(uint8_t deviceIndex = 0);
    /// True if the specified gamepad button is currently pressed.
    static bool IsGamepadButtonDown(GamepadButton::Code button, uint8_t deviceIndex = 0);
    /// True only on the frame the button transitioned from up to down.
    static bool WasGamepadButtonPressedThisFrame(GamepadButton::Code button, uint8_t deviceIndex = 0);
    /// True only on the frame the button transitioned from down to up.
    static bool WasGamepadButtonReleasedThisFrame(GamepadButton::Code button, uint8_t deviceIndex = 0);
    /// Returns the current value of the specified gamepad axis.
    static float GetGamepadAxis(GamepadAxis::Code axis, uint8_t deviceIndex = 0);

    // --- Capture flags (set by ImGui layer) ---
    static void SetKeyboardCaptured(bool captured);
    static void SetMouseCaptured(bool captured);
    static bool IsKeyboardCaptured();
    static bool IsMouseCaptured();

private:
    friend struct TestSupport::InputTestAccess;

    static void EnsureDevices(GLFWwindow* window);
    static void ResetDevices();
    static void ApplyPolledState(const PolledState& state);
    static void ApplyGamepadState(uint8_t deviceIndex, const GamepadPolledState& state);

    /// Non-owning pointer. Returns null when the current keyboard slot is not backed
    /// by a concrete KeyboardDevice instance (for example, after swapping in a custom device).
    static KeyboardDevice* GetKeyboardDevice();
    /// Non-owning pointer. Returns null when the current mouse slot is not backed
    /// by a concrete MouseDevice instance.
    static MouseDevice* GetMouseDevice();
    /// Non-owning pointer. Returns null when the requested gamepad slot is absent or
    /// is not backed by a concrete GamepadDevice instance.
    static GamepadDevice* GetGamepadDevice(uint8_t deviceIndex);

private:
    static GLFWwindow* s_Window; // Non-owning. Lifetime is managed by Window/Application.
    static EventBus* s_EventBus; // Non-owning. Lifetime is managed by Application.
    static Scope<InputDeviceManager> s_DeviceManager;

    static bool s_KeyboardCaptured;
    static bool s_MouseCaptured;
};
