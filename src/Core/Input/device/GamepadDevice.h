#pragma once

#include <array>

#include "core/input/code/GamepadCode.h"
#include "core/input/device/InputDevice.h"

class GamepadDevice final : public InputDevice
{
public:
    explicit GamepadDevice(uint8_t deviceIndex);

    Type GetType() const override { return Type::Gamepad; }
    void Poll() override;

    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    InputValue GetAxis(uint16_t axisId) const override;
    InputValue GetPreviousAxis(uint16_t axisId) const override;

    bool IsConnected() const override { return m_Connected; }
    bool WasConnected() const override { return m_PreviousConnected; }
    bool HasConnectionStateChanged() const override { return m_HasConnectionStateChanged; }
    uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }
    void Reset() override;
    void ApplyState(bool connected,
                    const std::array<bool, GamepadButton::Count> &buttons,
                    const std::array<float, GamepadAxis::Count> &axes);

private:
    uint8_t m_DeviceIndex = 0;
    bool m_Connected = false;
    bool m_PreviousConnected = false;
    bool m_HasConnectionStateChanged = false;
    bool m_HasPolledOnce = false;

    std::array<bool, GamepadButton::Count> m_CurrentButtons{};
    std::array<bool, GamepadButton::Count> m_PreviousButtons{};
    std::array<float, GamepadAxis::Count> m_CurrentAxes{};
    std::array<float, GamepadAxis::Count> m_PreviousAxes{};
};
