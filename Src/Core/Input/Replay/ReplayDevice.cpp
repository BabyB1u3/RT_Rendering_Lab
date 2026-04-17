#include "Core/Input/Replay/ReplayDevice.h"

#include "Core/Input/Device/MouseDevice.h"

namespace
{
InputValue BoolToValue(bool down)
{
    return {down ? 1.0f : 0.0f, 0.0f};
}

bool IsKeyboardKeyDown(const RecordedKeyboardState* state, uint16_t code)
{
    return state && code < state->keys.size() && state->keys[code] != 0;
}

bool IsMouseButtonDown(const RecordedMouseState* state, uint16_t code)
{
    return state && code < state->buttons.size() && state->buttons[code] != 0;
}

bool IsGamepadButtonDown(const RecordedGamepadState* state, uint16_t code)
{
    return state && code < state->buttons.size() && state->buttons[code] != 0;
}

float GetGamepadAxisValue(const RecordedGamepadState* state, uint16_t axisId)
{
    return (state && axisId < state->axes.size()) ? state->axes[axisId] : 0.0f;
}
} // namespace

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
    const auto* state = m_Session.GetKeyboardState();
    return state ? state->isConnected : false;
}

bool ReplayKeyboardDevice::WasConnected() const
{
    const auto* state = m_Session.GetPreviousKeyboardState();
    return state ? state->isConnected : false;
}

bool ReplayKeyboardDevice::HasConnectionStateChanged() const
{
    const auto* previous = m_Session.GetPreviousKeyboardState();
    return previous != nullptr && IsConnected() != WasConnected();
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
    const auto* current = m_Session.GetMouseState();
    const auto* previous = m_Session.GetPreviousMouseState();

    switch (axisId)
    {
        case MouseAxis::PositionX:
            return {current ? current->x : (previous ? previous->x : 0.0f), 0.0f};
        case MouseAxis::PositionY:
            return {current ? current->y : (previous ? previous->y : 0.0f), 0.0f};
        case MouseAxis::DeltaX:
            return {(current && previous) ? (current->x - previous->x) : 0.0f, 0.0f};
        case MouseAxis::DeltaY:
            return {(current && previous) ? (current->y - previous->y) : 0.0f, 0.0f};
        case MouseAxis::ScrollY:
            return {current ? current->scrollDelta : 0.0f, 0.0f};
        default:
            return {};
    }
}

InputValue ReplayMouseDevice::GetPreviousAxis(uint16_t axisId) const
{
    const auto* previous = m_Session.GetPreviousMouseState();
    const auto* beforePrevious = m_Session.GetMouseStateBeforePrevious();

    switch (axisId)
    {
        case MouseAxis::PositionX:
            return {previous ? previous->x : 0.0f, 0.0f};
        case MouseAxis::PositionY:
            return {previous ? previous->y : 0.0f, 0.0f};
        case MouseAxis::DeltaX:
            return {(previous && beforePrevious) ? (previous->x - beforePrevious->x) : 0.0f, 0.0f};
        case MouseAxis::DeltaY:
            return {(previous && beforePrevious) ? (previous->y - beforePrevious->y) : 0.0f, 0.0f};
        case MouseAxis::ScrollY:
            return {previous ? previous->scrollDelta : 0.0f, 0.0f};
        default:
            return {};
    }
}

bool ReplayMouseDevice::IsConnected() const
{
    const auto* state = m_Session.GetMouseState();
    return state ? state->isConnected : false;
}

bool ReplayMouseDevice::WasConnected() const
{
    const auto* state = m_Session.GetPreviousMouseState();
    return state ? state->isConnected : false;
}

bool ReplayMouseDevice::HasConnectionStateChanged() const
{
    const auto* previous = m_Session.GetPreviousMouseState();
    return previous != nullptr && IsConnected() != WasConnected();
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
    const auto* state = m_Session.GetGamepadState(m_DeviceIndex);
    return state ? state->isConnected : false;
}

bool ReplayGamepadDevice::WasConnected() const
{
    const auto* state = m_Session.GetPreviousGamepadState(m_DeviceIndex);
    return state ? state->isConnected : false;
}

bool ReplayGamepadDevice::HasConnectionStateChanged() const
{
    const auto* previous = m_Session.GetPreviousGamepadState(m_DeviceIndex);
    return previous != nullptr && IsConnected() != WasConnected();
}
