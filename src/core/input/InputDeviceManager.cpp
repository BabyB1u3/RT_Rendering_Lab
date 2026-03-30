#include "core/input/InputDeviceManager.h"

#include "core/event/EventBus.h"
#include "core/event/Events.h"

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
}

void InputDeviceManager::AddDevice(std::unique_ptr<InputDevice> device)
{
    if (!device)
        return;

    auto existing = FindDeviceBySlot(m_Devices.begin(), m_Devices.end(), device->GetType(), device->GetDeviceIndex());
    if (existing != m_Devices.end())
    {
        *existing = std::move(device);
        return;
    }

    m_Devices.push_back(std::move(device));
}

void InputDeviceManager::Clear()
{
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
        if (device->GetType() != InputDevice::Type::Gamepad || !device->HasConnectionStateChanged())
            continue;

        if (device->IsConnected())
            m_EventBus->Publish(GamepadConnectedEvent{device->GetDeviceIndex()});
        else
            m_EventBus->Publish(GamepadDisconnectedEvent{device->GetDeviceIndex()});
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

std::unique_ptr<InputDevice> InputDeviceManager::RemoveDevice(InputDevice::Type type, uint8_t index)
{
    const auto it = FindDeviceBySlot(m_Devices.begin(), m_Devices.end(), type, index);
    if (it == m_Devices.end())
        return nullptr;

    auto removed = std::move(*it);
    m_Devices.erase(it);
    return removed;
}
