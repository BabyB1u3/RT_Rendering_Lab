#pragma once

/// @file InputAction.h
/// @brief Input Action Map - decouples game logic from physical key bindings.
///
/// Provides two abstractions:
///   - **Actions** (discrete, boolean): "ShowShadowMap", "ToggleLookMode"
///   - **Axes** (continuous, float [-1,+1] or raw delta): "MoveForward", "LookX"
///
/// Each demo/layer owns its own InputActionMap instance with its own bindings.

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/input/KeyCode.h"
#include "core/input/MouseCode.h"
#include "core/input/InputModifier.h"
#include "core/input/InputTrigger.h"

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

    /// Remove all actions and axes (also clears modifiers and triggers).
    void Clear();

    /// True if this map defines the named action.
    bool HasAction(const std::string &name) const;

    /// True if this map defines the named axis.
    bool HasAxis(const std::string &name) const;

    // --- Modifiers & Triggers ---

    /// Add a modifier to an axis. Modifiers are applied in insertion order.
    void AddModifier(const std::string &axisName, std::unique_ptr<InputModifier> modifier);

    /// Set the trigger for an action. Replaces any existing trigger.
    /// Pass nullptr to revert to default (pressed) behavior.
    void SetTrigger(const std::string &actionName, std::unique_ptr<InputTrigger> trigger);

    /// Per-frame update: applies axis modifiers and advances trigger state machines.
    /// Must be called once per frame if modifiers or triggers are in use.
    void Update(float dt);

    // --- Queries (called per frame in OnUpdate) ---

    /// True if ANY bound source for this action is currently held.
    bool IsActionDown(const std::string &name) const;

    /// True only on the frame the action transitions from up to down.
    bool WasActionPressedThisFrame(const std::string &name) const;

    /// True only on the frame the action transitions from down to up.
    bool WasActionReleasedThisFrame(const std::string &name) const;

    /// Returns a float in [-1, +1] for key axes, or raw delta for mouse axes.
    /// If modifiers are attached and Update() has been called, returns the modified value.
    float GetAxis(const std::string &name) const;

    /// True if the action's trigger condition was met this frame.
    /// Requires Update(dt) to have been called. Falls back to WasActionPressedThisFrame()
    /// if no trigger is set.
    bool WasActionTriggeredThisFrame(const std::string &name) const;

    /// Returns the current trigger state for an action (None, Ongoing, or Triggered).
    TriggerState GetActionTriggerState(const std::string &name) const;

    /// Reset cached runtime state and trigger state machines.
    /// Used when a context is deactivated so stale state does not survive re-enable.
    void ResetRuntimeState();

    // --- Internal types (public for serialization traits) ---

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

    // --- Read access for serialization ---

    const std::unordered_map<std::string, std::vector<InputSource>> &GetActions() const { return m_Actions; }
    const std::unordered_map<std::string, AxisEntry> &GetAxes() const { return m_Axes; }

private:
    /// Check if a single InputSource is currently down.
    static bool IsSourceDown(const InputSource &source);
    /// Check if a single InputSource was pressed this frame.
    static bool WasSourcePressedThisFrame(const InputSource &source);
    /// Check if a single InputSource was released this frame.
    static bool WasSourceReleasedThisFrame(const InputSource &source);

    /// Compute the raw (unmodified) axis value.
    float ComputeRawAxis(const AxisEntry &entry) const;

    // Action name -> list of sources (OR: any one triggers the action)
    std::unordered_map<std::string, std::vector<InputSource>> m_Actions;

    // Axis name -> binding
    std::unordered_map<std::string, AxisEntry> m_Axes;

    // Axis name -> ordered modifier chain
    std::unordered_map<std::string, std::vector<std::unique_ptr<InputModifier>>> m_Modifiers;

    // Action name -> trigger (nullptr = default pressed behavior)
    std::unordered_map<std::string, std::unique_ptr<InputTrigger>> m_Triggers;

    // Cached trigger states, updated by Update()
    std::unordered_map<std::string, TriggerState> m_TriggerStates;

    // Cached modified axis values, updated by Update()
    std::unordered_map<std::string, float> m_CachedAxisValues;
};
