#pragma once

/// @file InputAction.h
/// @brief Input Action Map - decouples game logic from physical key bindings.
///
/// Provides two abstractions:
///   - **Actions** (discrete, boolean): "ShowShadowMap", "ToggleLookMode"
///   - **Axes** (continuous, float [-1,+1] or raw delta): "MoveForward", "LookX"
///
/// Each demo/layer owns its own InputActionMap instance with its own bindings.

#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Base.h"
#include "Core/Input/Action/InputModifier.h"
#include "Core/Input/Action/InputPatterns.h"
#include "Core/Input/Action/InputSource.h"
#include "Core/Input/Action/InputTrigger.h"

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
    /// Bind an action to a chord. All listed sources must be active simultaneously.
    void BindChordAction(const std::string &name, std::vector<InputSource> sources);
    void BindChordAction(const std::string &name, std::initializer_list<InputSource> sources);

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

    /// Bind a 1D axis directly to a gamepad axis.
    void BindAxis(const std::string &name, GamepadAxis::Code gamepadAxis, uint8_t deviceIndex = 0);

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
    void AddModifier(const std::string &axisName, Scope<InputModifier> modifier);

    /// Set the trigger for an action. Replaces any existing trigger.
    /// Pass nullptr to revert to default (pressed) behavior.
    void SetTrigger(const std::string &actionName, Scope<InputTrigger> trigger);

    /// Per-frame update: applies axis modifiers and advances trigger state machines.
    /// Must be called once per frame if modifiers or triggers are in use.
    void Update(float dt);
    /// Internal-use overload for InputContextStack chord blocking.
    void Update(float dt, const std::vector<InputSource> &blockedSources);

    // --- Queries (called per frame in OnUpdate) ---

    /// True if ANY bound source for this action is currently held.
    bool IsActionDown(const std::string &name) const;
    bool IsActionDown(const std::string &name, const std::vector<InputSource> &blockedSources) const;

    /// True only on the frame the action transitions from up to down.
    bool WasActionPressedThisFrame(const std::string &name) const;
    bool WasActionPressedThisFrame(const std::string &name, const std::vector<InputSource> &blockedSources) const;

    /// True only on the frame the action transitions from down to up.
    bool WasActionReleasedThisFrame(const std::string &name) const;
    bool WasActionReleasedThisFrame(const std::string &name, const std::vector<InputSource> &blockedSources) const;

    /// Returns a float in [-1, +1] for key axes, or raw delta for mouse axes.
    /// If modifiers are attached and Update() has been called, returns the modified value.
    float GetAxis(const std::string &name) const;

    /// True if the action's trigger condition was met this frame.
    /// Requires Update(dt) to have been called. Falls back to WasActionPressedThisFrame()
    /// if no trigger is set.
    bool WasActionTriggeredThisFrame(const std::string &name) const;
    bool WasActionTriggeredThisFrame(const std::string &name, const std::vector<InputSource> &blockedSources) const;

    /// Returns the current trigger state for an action (None, Ongoing, or Triggered).
    TriggerState GetActionTriggerState(const std::string &name) const;

    /// Reset cached runtime state and trigger state machines.
    /// Used when a context is deactivated so stale state does not survive re-enable.
    void ResetRuntimeState();

    /// True when this action still has at least one binding available after source masking.
    bool HasActionAvailable(const std::string &name, const std::vector<InputSource> &blockedSources) const;

    /// Append the sources of currently engaged chord bindings so lower-priority contexts
    /// can suppress conflicting single-key or shared-source actions.
    void AppendBlockingChordSources(std::vector<InputSource> &blockedSources) const;

    // --- Internal types (public for serialization traits) ---

    struct AxisEntry
    {
        enum class Kind
        {
            KeyPair,
            MouseAxis,
            GamepadAxis
        };
        Kind kind;
        AxisBinding keyPair; // used when kind == KeyPair
        MouseAxis mouseAxis; // used when kind == MouseAxis
        GamepadAxis::Code gamepadAxis = GamepadAxis::LeftX;
        uint8_t deviceIndex = 0;
    };

    // --- Read access for serialization ---

    const std::unordered_map<std::string, std::vector<InputSource>> &GetActions() const { return m_Actions; }
    const std::unordered_map<std::string, std::vector<ChordBinding>> &GetChordActions() const { return m_ChordActions; }
    const std::unordered_map<std::string, AxisEntry> &GetAxes() const { return m_Axes; }

private:
    /// Compute the raw (unmodified) axis value.
    float ComputeRawAxis(const AxisEntry &entry) const;

    struct ActionState
    {
        bool HasAvailableBinding = false;
        bool Down = false;
        bool Pressed = false;
        bool Released = false;
    };

    ActionState EvaluateActionState(const std::string &name, const std::vector<InputSource> &blockedSources) const;
    static bool IsChordDown(const ChordBinding &binding);
    static bool WasChordDown(const ChordBinding &binding);
    static bool IsChordBlocking(const ChordBinding &binding);
    static bool IsChordBlocked(const ChordBinding &binding, const std::vector<InputSource> &blockedSources);

    // Action name -> list of sources (OR: any one triggers the action)
    std::unordered_map<std::string, std::vector<InputSource>> m_Actions;
    // Action name -> list of chord bindings (OR: any one chord triggers the action)
    std::unordered_map<std::string, std::vector<ChordBinding>> m_ChordActions;

    // Axis name -> binding
    std::unordered_map<std::string, AxisEntry> m_Axes;

    // Axis name -> ordered modifier chain
    std::unordered_map<std::string, std::vector<Scope<InputModifier>>> m_Modifiers;

    // Action name -> trigger (nullptr = default pressed behavior)
    std::unordered_map<std::string, Scope<InputTrigger>> m_Triggers;

    // Cached trigger states, updated by Update()
    std::unordered_map<std::string, TriggerState> m_TriggerStates;

    // Cached modified axis values, updated by Update()
    std::unordered_map<std::string, float> m_CachedAxisValues;

    // Cached action runtime states, updated by Update(). This keeps default press/release
    // semantics aligned with stack-driven source masking for the current frame.
    std::unordered_map<std::string, ActionState> m_CachedActionStates;
};
