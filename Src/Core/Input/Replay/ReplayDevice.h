#pragma once

#include "Core/Input/Device/InputDevice.h"
#include "Core/Input/Replay/InputRecording.h"

class ReplayDevice : public InputDevice
{
public:
    explicit ReplayDevice(const InputReplaySession& session, uint8_t deviceIndex = 0)
        : m_Session(session), m_DeviceIndex(deviceIndex)
    {
    }

    uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }

protected:
    const InputReplaySession& m_Session;
    uint8_t m_DeviceIndex = 0;
};

class ReplayKeyboardDevice final : public ReplayDevice
{
public:
    explicit ReplayKeyboardDevice(const InputReplaySession& session) : ReplayDevice(session, 0) {}

    Type GetType() const override { return Type::Keyboard; }
    void Poll() override {}
    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    bool IsConnected() const override;
    bool WasConnected() const override;
};

class ReplayMouseDevice final : public ReplayDevice
{
public:
    explicit ReplayMouseDevice(const InputReplaySession& session) : ReplayDevice(session, 0) {}

    Type GetType() const override { return Type::Mouse; }
    void Poll() override {}
    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    InputValue GetAxis(uint16_t axisId) const override;
    InputValue GetPreviousAxis(uint16_t axisId) const override;
    bool IsConnected() const override;
    bool WasConnected() const override;
};

class ReplayGamepadDevice final : public ReplayDevice
{
public:
    ReplayGamepadDevice(const InputReplaySession& session, uint8_t deviceIndex) : ReplayDevice(session, deviceIndex) {}

    Type GetType() const override { return Type::Gamepad; }
    void Poll() override {}
    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    InputValue GetAxis(uint16_t axisId) const override;
    InputValue GetPreviousAxis(uint16_t axisId) const override;
    bool IsConnected() const override;
    bool WasConnected() const override;
};
