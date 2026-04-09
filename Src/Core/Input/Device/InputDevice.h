#pragma once

#include <cstdint>

struct InputValue
{
    float X = 0.0f;
    float Y = 0.0f;
};

class InputDevice
{
public:
    enum class Type : uint8_t
    {
        Keyboard,
        Mouse,
        Gamepad,
        Touch,
        Custom
    };

    virtual ~InputDevice() = default;

    virtual Type GetType() const = 0;
    virtual void Poll() = 0;

    virtual InputValue GetInput(uint16_t code) const = 0;
    virtual InputValue GetPreviousInput(uint16_t) const { return {}; }

    virtual InputValue GetAxis(uint16_t) const { return {}; }
    virtual InputValue GetPreviousAxis(uint16_t) const { return {}; }

    virtual bool IsConnected() const { return true; }
    virtual bool WasConnected() const { return IsConnected(); }
    virtual bool HasConnectionStateChanged() const { return false; }
    virtual uint8_t GetDeviceIndex() const { return 0; }
    virtual void Reset() {}
};
