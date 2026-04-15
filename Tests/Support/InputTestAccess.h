#pragma once

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Input.h"

namespace test_support
{
struct InputTestAccess
{
    static void RestoreDefaultDevices() { Input::RestoreDefaultDevices(); }

    using FrameState = Input::PolledState;

    static FrameState MakeFrame() { return {}; }

    static void SetKey(FrameState& state, Key::Code key, bool down)
    {
        if (key < Input::KEY_STATE_SIZE)
            state.Keys[key] = down;
    }

    static void SetMouseButton(FrameState& state, Mouse::Code button, bool down)
    {
        if (button < Input::MOUSE_BUTTON_COUNT)
            state.MouseButtons[button] = down;
    }

    static void SetMousePosition(FrameState& state, float x, float y)
    {
        state.MouseX = x;
        state.MouseY = y;
    }

    static void ApplyFrame(const FrameState& state) { Input::ApplyPolledState(state); }

    using GamepadFrameState = Input::GamepadPolledState;

    static GamepadFrameState MakeGamepadFrame(bool connected = false)
    {
        GamepadFrameState state;
        state.Connected = connected;
        return state;
    }

    static void SetGamepadButton(GamepadFrameState& state, GamepadButton::Code button, bool down)
    {
        if (button < GamepadButton::Count)
            state.Buttons[button] = down;
    }

    static void SetGamepadAxis(GamepadFrameState& state, GamepadAxis::Code axis, float value)
    {
        if (axis < GamepadAxis::Count)
            state.Axes[axis] = value;
    }

    static void ApplyGamepadFrame(uint8_t deviceIndex, const GamepadFrameState& state)
    {
        Input::ApplyGamepadState(deviceIndex, state);
    }
};
} // namespace test_support
