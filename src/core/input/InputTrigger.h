#pragma once

/// @file InputTrigger.h
/// @brief Input triggers - control *when* an action transitions to "triggered" state.
///
/// Without a trigger, an action fires the instant its source is pressed (default
/// PressedTrigger behavior). Triggers add conditions like "hold for 0.5s" or
/// "tap and release within 0.2s".
///
/// Built-in triggers: PressedTrigger, HoldTrigger, TapTrigger, ReleasedTrigger.

#include <cstdint>

/// Result of evaluating a trigger this frame.
enum class TriggerState : uint8_t
{
    None,     ///< Trigger condition not met.
    Ongoing,  ///< In progress (e.g., holding but haven't reached threshold).
    Triggered ///< Condition met this frame.
};

/// Base class for input triggers.
class InputTrigger
{
public:
    virtual ~InputTrigger() = default;

    /// Evaluate the trigger given the current action state.
    /// @param down     Is the physical source currently held?
    /// @param pressed  Was it just pressed this frame?
    /// @param released Was it just released this frame?
    /// @param dt       Frame delta time.
    virtual TriggerState Evaluate(bool down, bool pressed, bool released, float dt) = 0;

    /// Reset internal state (called when context is deactivated).
    virtual void Reset() {}
};

// ---- Built-in triggers ----

/// Default: triggers on press.
class PressedTrigger : public InputTrigger
{
public:
    TriggerState Evaluate(bool /*down*/, bool pressed, bool /*released*/, float /*dt*/) override
    {
        return pressed ? TriggerState::Triggered : TriggerState::None;
    }
};

/// Triggers after the source has been held for `duration` seconds.
class HoldTrigger : public InputTrigger
{
public:
    explicit HoldTrigger(float duration) : m_Duration(duration) {}

    TriggerState Evaluate(bool down, bool /*pressed*/, bool /*released*/, float dt) override
    {
        if (!down)
        {
            m_Elapsed = 0.0f;
            m_Fired = false;
            return TriggerState::None;
        }

        m_Elapsed += dt;

        if (!m_Fired && m_Elapsed >= m_Duration)
        {
            m_Fired = true;
            return TriggerState::Triggered;
        }

        return m_Fired ? TriggerState::None : TriggerState::Ongoing;
    }

    void Reset() override
    {
        m_Elapsed = 0.0f;
        m_Fired = false;
    }

private:
    float m_Duration;
    float m_Elapsed = 0.0f;
    bool m_Fired = false;
};

/// Triggers when the source is pressed and released within `maxDuration` seconds.
class TapTrigger : public InputTrigger
{
public:
    explicit TapTrigger(float maxDuration = 0.2f) : m_MaxDuration(maxDuration) {}

    TriggerState Evaluate(bool /*down*/, bool pressed, bool released, float dt) override
    {
        if (pressed)
        {
            m_Elapsed = 0.0f;
            m_Active = true;
            return TriggerState::Ongoing;
        }

        if (m_Active)
        {
            m_Elapsed += dt;

            if (released && m_Elapsed <= m_MaxDuration)
            {
                m_Active = false;
                return TriggerState::Triggered;
            }

            if (released || m_Elapsed > m_MaxDuration)
            {
                m_Active = false;
                return TriggerState::None;
            }

            return TriggerState::Ongoing;
        }

        return TriggerState::None;
    }

    void Reset() override
    {
        m_Elapsed = 0.0f;
        m_Active = false;
    }

private:
    float m_MaxDuration;
    float m_Elapsed = 0.0f;
    bool m_Active = false;
};

/// Triggers on release.
class ReleasedTrigger : public InputTrigger
{
public:
    TriggerState Evaluate(bool /*down*/, bool /*pressed*/, bool released, float /*dt*/) override
    {
        return released ? TriggerState::Triggered : TriggerState::None;
    }
};
