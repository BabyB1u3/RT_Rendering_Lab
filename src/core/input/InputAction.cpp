#include "core/input/InputAction.h"
#include "core/input/Input.h"

// --- Registration ---

void InputActionMap::BindAction(const std::string &name, InputSource source)
{
    m_Actions[name].push_back(source);
}

void InputActionMap::BindAction(const std::string &name, Key::Code key)
{
    BindAction(name, InputSource::FromKey(key));
}

void InputActionMap::BindAxis(const std::string &name, Key::Code positive, Key::Code negative)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::KeyPair;
    entry.keyPair.Positive = InputSource::FromKey(positive);
    entry.keyPair.Negative = InputSource::FromKey(negative);
    m_Axes[name] = entry;
}

void InputActionMap::BindAxis(const std::string &name, MouseAxis mouseAxis)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::MouseAxis;
    entry.mouseAxis = mouseAxis;
    m_Axes[name] = entry;
}

void InputActionMap::Unbind(const std::string &name)
{
    m_Actions.erase(name);
    m_Axes.erase(name);
    m_Modifiers.erase(name);
    m_Triggers.erase(name);
    m_TriggerStates.erase(name);
    m_CachedAxisValues.erase(name);
}

void InputActionMap::Clear()
{
    m_Actions.clear();
    m_Axes.clear();
    m_Modifiers.clear();
    m_Triggers.clear();
    m_TriggerStates.clear();
    m_CachedAxisValues.clear();
}

bool InputActionMap::HasAction(const std::string &name) const
{
    return m_Actions.find(name) != m_Actions.end();
}

bool InputActionMap::HasAxis(const std::string &name) const
{
    return m_Axes.find(name) != m_Axes.end();
}

void InputActionMap::AddModifier(const std::string &axisName, std::unique_ptr<InputModifier> modifier)
{
    m_Modifiers[axisName].push_back(std::move(modifier));
}

void InputActionMap::SetTrigger(const std::string &actionName, std::unique_ptr<InputTrigger> trigger)
{
    if (trigger)
    {
        m_Triggers[actionName] = std::move(trigger);
        m_TriggerStates.erase(actionName);
    }
    else
    {
        m_Triggers.erase(actionName);
        m_TriggerStates.erase(actionName);
    }
}

void InputActionMap::Update(float dt)
{
    m_LastDt = dt;

    // Evaluate axis modifiers and cache results.
    m_CachedAxisValues.clear();
    for (const auto &[name, entry] : m_Axes)
    {
        float value = ComputeRawAxis(entry);

        auto modIt = m_Modifiers.find(name);
        if (modIt != m_Modifiers.end())
        {
            for (const auto &mod : modIt->second)
                value = mod->Apply(value, dt);
        }

        m_CachedAxisValues[name] = value;
    }

    // Advance trigger state machines.
    for (auto &[name, trigger] : m_Triggers)
    {
        auto actIt = m_Actions.find(name);
        if (actIt == m_Actions.end())
        {
            m_TriggerStates[name] = TriggerState::None;
            continue;
        }

        bool down = false;
        bool pressed = false;
        bool released = false;
        for (const auto &src : actIt->second)
        {
            if (IsSourceDown(src))
                down = true;
            if (WasSourcePressedThisFrame(src))
                pressed = true;
            if (WasSourceReleasedThisFrame(src))
                released = true;
        }

        m_TriggerStates[name] = trigger->Evaluate(down, pressed, released, dt);
    }
}

// --- Queries ---

bool InputActionMap::IsActionDown(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (IsSourceDown(src))
            return true;
    }
    return false;
}

bool InputActionMap::WasActionPressedThisFrame(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (WasSourcePressedThisFrame(src))
            return true;
    }
    return false;
}

bool InputActionMap::WasActionReleasedThisFrame(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (WasSourceReleasedThisFrame(src))
            return true;
    }
    return false;
}

float InputActionMap::GetAxis(const std::string &name) const
{
    // If Update() was called this frame, return the cached (modifier-applied) value.
    auto cachedIt = m_CachedAxisValues.find(name);
    if (cachedIt != m_CachedAxisValues.end())
        return cachedIt->second;

    // Fall back to raw computation (no modifiers, or Update() not called).
    auto it = m_Axes.find(name);
    if (it == m_Axes.end())
        return 0.0f;

    return ComputeRawAxis(it->second);
}

bool InputActionMap::WasActionTriggeredThisFrame(const std::string &name) const
{
    auto it = m_TriggerStates.find(name);
    if (it != m_TriggerStates.end())
        return it->second == TriggerState::Triggered;

    if (m_Triggers.find(name) != m_Triggers.end())
        return false;

    // No trigger set - fall back to default pressed behavior.
    return WasActionPressedThisFrame(name);
}

TriggerState InputActionMap::GetActionTriggerState(const std::string &name) const
{
    auto it = m_TriggerStates.find(name);
    if (it != m_TriggerStates.end())
        return it->second;

    if (m_Triggers.find(name) != m_Triggers.end())
        return TriggerState::None;

    // No trigger set - emulate pressed trigger.
    return WasActionPressedThisFrame(name) ? TriggerState::Triggered : TriggerState::None;
}

void InputActionMap::ResetRuntimeState()
{
    m_CachedAxisValues.clear();
    m_TriggerStates.clear();
    m_LastDt = 0.0f;

    for (auto &[name, trigger] : m_Triggers)
    {
        if (trigger)
            trigger->Reset();
    }
}

float InputActionMap::ComputeRawAxis(const AxisEntry &entry) const
{
    if (entry.kind == AxisEntry::Kind::KeyPair)
    {
        float value = 0.0f;
        if (IsSourceDown(entry.keyPair.Positive))
            value += 1.0f;
        if (IsSourceDown(entry.keyPair.Negative))
            value -= 1.0f;
        return value;
    }

    // MouseAxis
    switch (entry.mouseAxis)
    {
    case MouseAxis::X:
    {
        auto [dx, dy] = Input::GetMouseDelta();
        return dx;
    }
    case MouseAxis::Y:
    {
        auto [dx, dy] = Input::GetMouseDelta();
        return dy;
    }
    case MouseAxis::ScrollY:
        return Input::GetScrollDelta();
    }

    return 0.0f;
}

// --- Source helpers ---

bool InputActionMap::IsSourceDown(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::IsKeyDown(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::IsMouseButtonDown(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}

bool InputActionMap::WasSourcePressedThisFrame(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::WasKeyPressedThisFrame(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::WasMouseButtonPressedThisFrame(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}

bool InputActionMap::WasSourceReleasedThisFrame(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::WasKeyReleasedThisFrame(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::WasMouseButtonReleasedThisFrame(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}
