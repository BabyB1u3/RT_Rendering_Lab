#include "Core/Input/Device/InputDeviceManager.h"

#include "Core/Event/EventBus.h"
#include "Core/Event/Events.h"

namespace
{
    template <typename Iterator>
    Iterator FindDeviceBySlot(Iterator begin, Iterator end, InputDevice::Type type, uint8_t index)
    {
        for (auto it = begin; it != end; ++it)
        {
            if ((*it)->GetType() == type && (*it)->GetDeviceIndex() == index)
                return it;
        }

        return end;
    }

    void PublishAttached(EventBus *bus, InputDevice::Type type, uint8_t index)
    {
        if (bus)
            bus->Publish(DeviceAttachedToSlotEvent{type, index});
    }

    void PublishDetached(EventBus *bus, InputDevice::Type type, uint8_t index)
    {
        if (bus)
            bus->Publish(DeviceDetachedFromSlotEvent{type, index});
    }

    void PublishConnectionChanged(EventBus *bus, InputDevice::Type type, uint8_t index, bool connected)
    {
        if (!bus)
            return;

        bus->Publish(DeviceConnectionChangedEvent{type, index, connected});

        if (type == InputDevice::Type::Gamepad)
        {
            if (connected)
                bus->Publish(GamepadConnectedEvent{index});
            else
                bus->Publish(GamepadDisconnectedEvent{index});
        }
    }
}

void InputDeviceManager::AddDevice(Scope<InputDevice> device)
{
    if (!device)
        return;

    const auto type = device->GetType();
    const auto index = device->GetDeviceIndex();
    const bool newConnected = device->IsConnected();

    auto existing = FindDeviceBySlot(m_Devices.begin(), m_Devices.end(), type, index);
    if (existing != m_Devices.end())
    {
        const bool oldConnected = (*existing)->IsConnected();

        PublishDetached(m_EventBus, type, index);
        *existing = std::move(device);

        PublishAttached(m_EventBus, type, index);
        if (oldConnected != newConnected)
            PublishConnectionChanged(m_EventBus, type, index, newConnected);
        return;
    }

    m_Devices.push_back(std::move(device));
    PublishAttached(m_EventBus, type, index);
    if (newConnected)
        PublishConnectionChanged(m_EventBus, type, index, true);
}

void InputDeviceManager::Clear()
{
    for (const auto &device : m_Devices)
    {
        PublishDetached(m_EventBus, device->GetType(), device->GetDeviceIndex());
        if (device->IsConnected())
            PublishConnectionChanged(m_EventBus, device->GetType(), device->GetDeviceIndex(), false);
    }

    m_Devices.clear();
}

void InputDeviceManager::PollAll()
{
    for (const auto &device : m_Devices)
        device->Poll();

    if (!m_EventBus)
        return;

    for (const auto &device : m_Devices)
    {
        if (!device->HasConnectionStateChanged())
            continue;

        PublishConnectionChanged(m_EventBus, device->GetType(), device->GetDeviceIndex(), device->IsConnected());
    }
}

void InputDeviceManager::ResetAll()
{
    for (const auto &device : m_Devices)
        device->Reset();
}

InputDevice *InputDeviceManager::GetDevice(InputDevice::Type type, uint8_t index) const
{
    const auto it = FindDeviceBySlot(m_Devices.begin(), m_Devices.end(), type, index);
    return it != m_Devices.end() ? it->get() : nullptr;
}

std::size_t InputDeviceManager::GetDeviceCount(InputDevice::Type type) const
{
    std::size_t count = 0;
    for (const auto &device : m_Devices)
    {
        if (device->GetType() == type)
            ++count;
    }

    return count;
}

Scope<InputDevice> InputDeviceManager::RemoveDevice(InputDevice::Type type, uint8_t index)
{
    const auto it = FindDeviceBySlot(m_Devices.begin(), m_Devices.end(), type, index);
    if (it == m_Devices.end())
        return nullptr;

    const bool wasConnected = (*it)->IsConnected();
    auto removed = std::move(*it);
    m_Devices.erase(it);

    PublishDetached(m_EventBus, type, index);
    if (wasConnected)
        PublishConnectionChanged(m_EventBus, type, index, false);

    return removed;
}
