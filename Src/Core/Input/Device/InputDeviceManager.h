#pragma once

#include <cstddef>
#include <vector>

#include "Core/Base.h"
#include "Core/Input/Device/InputDevice.h"

class EventBus;
class InputDeviceManager;

class InputDeviceManagerObserver
{
public:
    virtual ~InputDeviceManagerObserver() = default;

    virtual void OnBeforePollAll(InputDeviceManager &, float) {}
    virtual void OnAfterPollAll(const InputDeviceManager &, float) {}
};

class InputDeviceManager
{
public:
    void AddDevice(Scope<InputDevice> device);
    void Clear();
    void PollAll(float dt = 0.0f);
    void ResetAll();
    void AddObserver(InputDeviceManagerObserver *observer);
    void RemoveObserver(InputDeviceManagerObserver *observer);

    /// Non-owning pointer. Returns null when the logical slot is not populated.
    InputDevice *GetDevice(InputDevice::Type type, uint8_t index = 0) const;
    std::size_t GetDeviceCount(InputDevice::Type type) const;
    std::vector<const InputDevice *> GetDevices(InputDevice::Type type) const;
    Scope<InputDevice> RemoveDevice(InputDevice::Type type, uint8_t index = 0);

    /// Non-owning pointer. Lifetime is managed by Application.
    void SetEventBus(EventBus *bus) { m_EventBus = bus; }

private:
    std::vector<Scope<InputDevice>> m_Devices;
    std::vector<InputDeviceManagerObserver *> m_Observers;
    EventBus *m_EventBus = nullptr; // Non-owning. Lifetime is managed by Application.
};
