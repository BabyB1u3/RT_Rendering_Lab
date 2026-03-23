#pragma once

/// @file InputAction.h
/// @brief Input Action Map — decouples game logic from physical key bindings.
///
/// Provides two abstractions:
///   - **Actions** (discrete, boolean): "ShowShadowMap", "ToggleLookMode"
///   - **Axes** (continuous, float [-1,+1] or raw delta): "MoveForward", "LookX"
///
/// Each demo/layer owns its own InputActionMap instance with its own bindings.

#include <string>
#include <vector>
#include <unordered_map>
#include "KeyCode.h"
#include "MouseCode.h"

/// An InputSource can be a key, mouse button, or (future) gamepad button.
struct InputSource
{
    enum class Type : uint8_t
    {
        Key,
        MouseButton,
        GamepadButton,
        GamepadAxis
    };

    Type SourceType;
    uint16_t Code; // Key::Code, Mouse::Code, or device-specific code
    uint8_t DeviceIndex = 0;

    static InputSource FromKey(Key::Code key)
    {
        return {Type::Key, key, 0};
    }

    static InputSource FromMouseButton(Mouse::Code button)
    {
        return {Type::MouseButton, button, 0};
    }
};

/// Binding for a 1D axis: positive source and negative source.
/// Value is +1 when positive is held, -1 when negative is held, 0 when neither/both.
struct AxisBinding
{
    InputSource Positive;
    InputSource Negative;
};

class InputActionMap
{
public:
    // --- Registration (typically in OnAttach or setup code) ---

    /// Bind an action name to an InputSource. Multiple calls with the same name
    /// ADD bindings (OR logic: any one triggers the action).
    void BindAction(const std::string &name, InputSource source);

    /// Convenience: bind action to a key.
    void BindAction(const std::string &name, Key::Code key);

    /// Convenience: bind action to a mouse button.
    void BindAction(const std::string &name, Mouse::Code button);

    /// Bind a 1D axis to a positive/negative key pair.
    void BindAxis(const std::string &name, Key::Code positive, Key::Code negative);

    /// Bind a 1D axis to mouse delta (X, Y, or ScrollY).
    enum class MouseAxis : uint8_t
    {
        X,
        Y,
        ScrollY
    };
    void BindAxis(const std::string &name, MouseAxis mouseAxis);

    /// Remove all bindings for a given action or axis name.
    void Unbind(const std::string &name);

    // --- Queries (called per frame in OnUpdate) ---

    /// True if ANY bound source for this action is currently held.
    bool IsActionDown(const std::string &name) const;

    /// True only on the frame the action transitions from up to down.
    bool WasActionPressedThisFrame(const std::string &name) const;

    /// True only on the frame the action transitions from down to up.
    bool WasActionReleasedThisFrame(const std::string &name) const;

    /// Returns a float in [-1, +1] for key axes, or raw delta for mouse axes.
    float GetAxis(const std::string &name) const;

private:
    /// Check if a single InputSource is currently down.
    static bool IsSourceDown(const InputSource &source);
    /// Check if a single InputSource was pressed this frame.
    static bool WasSourcePressedThisFrame(const InputSource &source);
    /// Check if a single InputSource was released this frame.
    static bool WasSourceReleasedThisFrame(const InputSource &source);

    // Action name -> list of sources (OR: any one triggers the action)
    std::unordered_map<std::string, std::vector<InputSource>> m_Actions;

    // Axis name -> binding
    struct AxisEntry
    {
        enum class Kind
        {
            KeyPair,
            MouseAxis
        };
        Kind kind;
        AxisBinding keyPair; // used when kind == KeyPair
        MouseAxis mouseAxis; // used when kind == MouseAxis
    };
    std::unordered_map<std::string, AxisEntry> m_Axes;
};
