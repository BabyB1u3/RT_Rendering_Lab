#include "Core/Input/Replay/ReplayDevice.h"

#include "Core/Input/Device/MouseDevice.h"

namespace
{
    InputValue BoolToValue(bool down)
    {
        return {down ? 1.0f : 0.0f, 0.0f};
    }

    bool IsKeyboardKeyDown(const RecordedKeyboardState *state, uint16_t code)
    {
        return state && code < state->Keys.size() && state->Keys[code] != 0;
    }

    bool IsMouseButtonDown(const RecordedMouseState *state, uint16_t code)
    {
        return state && code < state->Buttons.size() && state->Buttons[code] != 0;
    }

    bool IsGamepadButtonDown(const RecordedGamepadState *state, uint16_t code)
    {
        return state && code < state->Buttons.size() && state->Buttons[code] != 0;
    }

    float GetGamepadAxisValue(const RecordedGamepadState *state, uint16_t axisId)
    {
        return (state && axisId < state->Axes.size()) ? state->Axes[axisId] : 0.0f;
    }
}

InputValue ReplayKeyboardDevice::GetInput(uint16_t code) const
{
    return BoolToValue(IsKeyboardKeyDown(m_Session.GetKeyboardState(), code));
}

InputValue ReplayKeyboardDevice::GetPreviousInput(uint16_t code) const
{
    return BoolToValue(IsKeyboardKeyDown(m_Session.GetPreviousKeyboardState(), code));
}

bool ReplayKeyboardDevice::IsConnected() const
{
    const auto *state = m_Session.GetKeyboardState();
    return state ? state->Connected : false;
}

bool ReplayKeyboardDevice::WasConnected() const
{
    const auto *state = m_Session.GetPreviousKeyboardState();
    return state ? state->Connected : false;
}

InputValue ReplayMouseDevice::GetInput(uint16_t code) const
{
    return BoolToValue(IsMouseButtonDown(m_Session.GetMouseState(), code));
}

InputValue ReplayMouseDevice::GetPreviousInput(uint16_t code) const
{
    return BoolToValue(IsMouseButtonDown(m_Session.GetPreviousMouseState(), code));
}

InputValue ReplayMouseDevice::GetAxis(uint16_t axisId) const
{
    const auto *current = m_Session.GetMouseState();
    const auto *previous = m_Session.GetPreviousMouseState();

    switch (axisId)
    {
    case MouseAxisId::PositionX:
        return {current ? current->X : (previous ? previous->X : 0.0f), 0.0f};
    case MouseAxisId::PositionY:
        return {current ? current->Y : (previous ? previous->Y : 0.0f), 0.0f};
    case MouseAxisId::DeltaX:
        return {(current && previous) ? (current->X - previous->X) : 0.0f, 0.0f};
    case MouseAxisId::DeltaY:
        return {(current && previous) ? (current->Y - previous->Y) : 0.0f, 0.0f};
    case MouseAxisId::ScrollY:
        return {current ? current->ScrollDelta : 0.0f, 0.0f};
    default:
        return {};
    }
}

InputValue ReplayMouseDevice::GetPreviousAxis(uint16_t axisId) const
{
    const auto *previous = m_Session.GetPreviousMouseState();
    const auto *beforePrevious = m_Session.GetMouseStateBeforePrevious();

    switch (axisId)
    {
    case MouseAxisId::PositionX:
        return {previous ? previous->X : 0.0f, 0.0f};
    case MouseAxisId::PositionY:
        return {previous ? previous->Y : 0.0f, 0.0f};
    case MouseAxisId::DeltaX:
        return {(previous && beforePrevious) ? (previous->X - beforePrevious->X) : 0.0f, 0.0f};
    case MouseAxisId::DeltaY:
        return {(previous && beforePrevious) ? (previous->Y - beforePrevious->Y) : 0.0f, 0.0f};
    case MouseAxisId::ScrollY:
        return {previous ? previous->ScrollDelta : 0.0f, 0.0f};
    default:
        return {};
    }
}

bool ReplayMouseDevice::IsConnected() const
{
    const auto *state = m_Session.GetMouseState();
    return state ? state->Connected : false;
}

bool ReplayMouseDevice::WasConnected() const
{
    const auto *state = m_Session.GetPreviousMouseState();
    return state ? state->Connected : false;
}

InputValue ReplayGamepadDevice::GetInput(uint16_t code) const
{
    return BoolToValue(IsGamepadButtonDown(m_Session.GetGamepadState(m_DeviceIndex), code));
}

InputValue ReplayGamepadDevice::GetPreviousInput(uint16_t code) const
{
    return BoolToValue(IsGamepadButtonDown(m_Session.GetPreviousGamepadState(m_DeviceIndex), code));
}

InputValue ReplayGamepadDevice::GetAxis(uint16_t axisId) const
{
    return {GetGamepadAxisValue(m_Session.GetGamepadState(m_DeviceIndex), axisId), 0.0f};
}

InputValue ReplayGamepadDevice::GetPreviousAxis(uint16_t axisId) const
{
    return {GetGamepadAxisValue(m_Session.GetPreviousGamepadState(m_DeviceIndex), axisId), 0.0f};
}

bool ReplayGamepadDevice::IsConnected() const
{
    const auto *state = m_Session.GetGamepadState(m_DeviceIndex);
    return state ? state->Connected : false;
}

bool ReplayGamepadDevice::WasConnected() const
{
    const auto *state = m_Session.GetPreviousGamepadState(m_DeviceIndex);
    return state ? state->Connected : false;
}
