#include "InputAction.h"
#include "Input.h"

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
    auto it = m_Axes.find(name);
    if (it == m_Axes.end())
        return 0.0f;

    const AxisEntry &entry = it->second;

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
