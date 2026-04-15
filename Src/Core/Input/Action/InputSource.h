#pragma once

#include <cstdint>
#include <vector>

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Code/KeyCode.h"
#include "Core/Input/Code/MouseCode.h"

/// An InputSource can be a key, mouse button, gamepad button, or gamepad axis.
struct InputSource
{
    enum class Type : uint8_t
    {
        Key,
        MouseButton,
        GamepadButton,
        GamepadAxis
    };

    Type SourceType = Type::Key;
    uint16_t Code = 0; // Key::Code, Mouse::Code, or device-specific code
    uint8_t DeviceIndex = 0;

    static InputSource FromKey(Key::Code key) { return {Type::Key, key, 0}; }

    static InputSource FromMouseButton(Mouse::Code button) { return {Type::MouseButton, button, 0}; }

    static InputSource FromGamepadButton(GamepadButton::Code button, uint8_t deviceIndex = 0)
    {
        return {Type::GamepadButton, button, deviceIndex};
    }

    static InputSource FromGamepadAxis(GamepadAxis::Code axis, uint8_t deviceIndex = 0)
    {
        return {Type::GamepadAxis, axis, deviceIndex};
    }
};

namespace InputSourceState
{
bool IsDown(const InputSource& source);
bool WasDown(const InputSource& source);
bool WasPressedThisFrame(const InputSource& source);
bool WasReleasedThisFrame(const InputSource& source);

bool Equals(const InputSource& lhs, const InputSource& rhs);
bool IsBlocked(const InputSource& source, const std::vector<InputSource>& blockedSources);
void AppendUnique(std::vector<InputSource>& dest, const InputSource& source);
} // namespace InputSourceState
