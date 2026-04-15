#pragma once

/// @file InputModifier.h
/// @brief Input value modifiers - transform raw axis values through a pipeline.
///
/// Modifiers are applied in insertion order: Raw → Modifier1 → Modifier2 → ... → Final.
/// Each modifier receives the current value and frame delta time, and returns
/// the transformed value.
///
/// Built-in modifiers: DeadZone, Sensitivity, Clamp, Negate, Smooth.

#include <cmath>
#include <algorithm>

/// Base class for input value modifiers.
class InputModifier
{
public:
    virtual ~InputModifier() = default;

    /// Transform the input value. deltaTime is provided for time-dependent modifiers.
    virtual float Apply(float value, float deltaTime) const = 0;
};

// ---- Built-in modifiers ----

/// Zeroes out values whose absolute magnitude is below the threshold.
class DeadZone : public InputModifier
{
public:
    explicit DeadZone(float threshold) : m_Threshold(threshold) {}

    float Apply(float value, float /*dt*/) const override { return std::abs(value) < m_Threshold ? 0.0f : value; }

private:
    float m_Threshold;
};

/// Multiplies the value by a scalar.
class Sensitivity : public InputModifier
{
public:
    explicit Sensitivity(float scale) : m_Scale(scale) {}

    float Apply(float value, float /*dt*/) const override { return value * m_Scale; }

private:
    float m_Scale;
};

/// Clamps value to [min, max].
class Clamp : public InputModifier
{
public:
    Clamp(float min, float max) : m_Min(min), m_Max(max) {}

    float Apply(float value, float /*dt*/) const override { return std::clamp(value, m_Min, m_Max); }

private:
    float m_Min;
    float m_Max;
};

/// Inverts the axis (multiply by -1).
class Negate : public InputModifier
{
public:
    float Apply(float value, float /*dt*/) const override { return -value; }
};

/// Smooths value over time using exponential moving average.
/// Useful for reducing jitter on mouse or gamepad stick input.
class Smooth : public InputModifier
{
public:
    explicit Smooth(float halfLife) : m_HalfLife(halfLife) {}

    float Apply(float value, float dt) const override
    {
        if (dt <= 0.0f)
            return value;
        float alpha = 1.0f - std::pow(2.0f, -dt / m_HalfLife);
        m_Smoothed = m_Smoothed + alpha * (value - m_Smoothed);
        return m_Smoothed;
    }

private:
    float m_HalfLife;
    mutable float m_Smoothed = 0.0f;
};
