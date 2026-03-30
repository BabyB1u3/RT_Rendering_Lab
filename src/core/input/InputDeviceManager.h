#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "core/input/InputDevice.h"

class EventBus;

class InputDeviceManager
{
public:
    void AddDevice(std::unique_ptr<InputDevice> device);
    void Clear();
    void PollAll();
    void ResetAll();

    InputDevice *GetDevice(InputDevice::Type type, uint8_t index = 0) const;
    std::size_t GetDeviceCount(InputDevice::Type type) const;
    std::unique_ptr<InputDevice> RemoveDevice(InputDevice::Type type, uint8_t index = 0);

    void SetEventBus(EventBus *bus) { m_EventBus = bus; }

private:
    std::vector<std::unique_ptr<InputDevice>> m_Devices;
    EventBus *m_EventBus = nullptr;
};
