#pragma once

#include "core/input/Input.h"

namespace test_support
{
struct InputTestAccess
{
    using FrameState = Input::PolledState;

    static FrameState MakeFrame()
    {
        return {};
    }

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

    static void ApplyFrame(const FrameState& state)
    {
        Input::ApplyPolledState(state);
    }
};
} // namespace test_support
