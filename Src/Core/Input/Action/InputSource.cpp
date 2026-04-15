#include "Core/Input/Action/InputSource.h"

#include "Core/Input/Device/InputDeviceManager.h"
#include "Core/Input/Input.h"

namespace
{
constexpr float kInputPressThreshold = 0.5f;

bool IsActive(float value)
{
    return value > kInputPressThreshold || value < -kInputPressThreshold;
}
} // namespace

namespace InputSourceState
{
bool IsDown(const InputSource& source)
{
    const auto* manager = Input::TryGetDeviceManager();
    if (!manager)
        return false;

    switch (source.SourceType)
    {
        case InputSource::Type::Key:
        {
            if (Input::IsKeyboardCaptured())
                return false;

            const auto* device = manager->GetDevice(InputDevice::Type::Keyboard);
            return device && device->GetInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::MouseButton:
        {
            if (Input::IsMouseCaptured())
                return false;

            const auto* device = manager->GetDevice(InputDevice::Type::Mouse);
            return device && device->GetInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::GamepadButton:
        {
            const auto* device = manager->GetDevice(InputDevice::Type::Gamepad, source.DeviceIndex);
            return device && device->GetInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::GamepadAxis:
        {
            const auto* device = manager->GetDevice(InputDevice::Type::Gamepad, source.DeviceIndex);
            return device && IsActive(device->GetAxis(source.Code).X);
        }
        default:
            return false;
    }
}

bool WasDown(const InputSource& source)
{
    const auto* manager = Input::TryGetDeviceManager();
    if (!manager)
        return false;

    switch (source.SourceType)
    {
        case InputSource::Type::Key:
        {
            if (Input::IsKeyboardCaptured())
                return false;

            const auto* device = manager->GetDevice(InputDevice::Type::Keyboard);
            return device && device->GetPreviousInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::MouseButton:
        {
            if (Input::IsMouseCaptured())
                return false;

            const auto* device = manager->GetDevice(InputDevice::Type::Mouse);
            return device && device->GetPreviousInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::GamepadButton:
        {
            const auto* device = manager->GetDevice(InputDevice::Type::Gamepad, source.DeviceIndex);
            return device && device->GetPreviousInput(source.Code).X > kInputPressThreshold;
        }
        case InputSource::Type::GamepadAxis:
        {
            const auto* device = manager->GetDevice(InputDevice::Type::Gamepad, source.DeviceIndex);
            return device && IsActive(device->GetPreviousAxis(source.Code).X);
        }
        default:
            return false;
    }
}

bool WasPressedThisFrame(const InputSource& source)
{
    return IsDown(source) && !WasDown(source);
}

bool WasReleasedThisFrame(const InputSource& source)
{
    return !IsDown(source) && WasDown(source);
}

bool Equals(const InputSource& lhs, const InputSource& rhs)
{
    return lhs.SourceType == rhs.SourceType && lhs.Code == rhs.Code && lhs.DeviceIndex == rhs.DeviceIndex;
}

bool IsBlocked(const InputSource& source, const std::vector<InputSource>& blockedSources)
{
    for (const auto& blocked : blockedSources)
    {
        if (Equals(source, blocked))
            return true;
    }

    return false;
}

void AppendUnique(std::vector<InputSource>& dest, const InputSource& source)
{
    if (!IsBlocked(source, dest))
        dest.push_back(source);
}
} // namespace InputSourceState
