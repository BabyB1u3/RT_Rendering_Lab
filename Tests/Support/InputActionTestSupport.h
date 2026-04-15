#pragma once

#include <memory>
#include <string>

#include "Core/Input/Action/InputAction.h"

namespace test_support
{
class FixedValueModifier final : public InputModifier
{
public:
    explicit FixedValueModifier(float value) : m_Value(value) {}

    float Apply(float /*value*/, float /*deltaTime*/) const override { return m_Value; }

private:
    float m_Value;
};

class ConstantTrigger final : public InputTrigger
{
public:
    explicit ConstantTrigger(TriggerState state, int* resetCount = nullptr) : m_State(state), m_ResetCount(resetCount)
    {
    }

    TriggerState Evaluate(bool /*down*/, bool /*pressed*/, bool /*released*/, float /*dt*/) override { return m_State; }

    void Reset() override
    {
        if (m_ResetCount != nullptr)
            ++(*m_ResetCount);
    }

private:
    TriggerState m_State;
    int* m_ResetCount;
};

inline void BindConstantAxis(InputActionMap& map, const std::string& name, float value)
{
    map.BindAxis(name, Key::D, Key::A);
    map.AddModifier(name, std::make_unique<FixedValueModifier>(value));
}

inline void
BindConstantTrigger(InputActionMap& map, const std::string& name, TriggerState state, int* resetCount = nullptr)
{
    map.BindAction(name, Key::Space);
    map.SetTrigger(name, std::make_unique<ConstantTrigger>(state, resetCount));
}
} // namespace test_support
