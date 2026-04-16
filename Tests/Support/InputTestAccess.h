#pragma once

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Input.h"

namespace TestSupport
{
struct InputTestAccess
{
    static void RestoreDefaultDevices() { Input::RestoreDefaultDevices(); }

    using FrameState = Input::PolledState;

    static FrameState MakeFrame() { return {}; }

    static void SetKey(FrameState& state, Key::Code key, bool down)
    {
        if (key < Input::k_KeyStateSize)
            state.keys[key] = down;
    }

    static void SetMouseButton(FrameState& state, Mouse::Code button, bool down)
    {
        if (button < Input::k_MouseButtonCount)
            state.mouseButtons[button] = down;
    }

    static void SetMousePosition(FrameState& state, float x, float y)
    {
        state.mouseX = x;
        state.mouseY = y;
    }

    static void ApplyFrame(const FrameState& state) { Input::ApplyPolledState(state); }

    using GamepadFrameState = Input::GamepadPolledState;

    static GamepadFrameState MakeGamepadFrame(bool connected = false)
    {
        GamepadFrameState state;
        state.isConnected = connected;
        return state;
    }

    static void SetGamepadButton(GamepadFrameState& state, GamepadButton::Code button, bool down)
    {
        if (button < GamepadButton::Count)
            state.buttons[button] = down;
    }

    static void SetGamepadAxis(GamepadFrameState& state, GamepadAxis::Code axis, float value)
    {
        if (axis < GamepadAxis::Count)
            state.axes[axis] = value;
    }

    static void ApplyGamepadFrame(uint8_t deviceIndex, const GamepadFrameState& state)
    {
        Input::ApplyGamepadState(deviceIndex, state);
    }
};
} // namespace TestSupport
