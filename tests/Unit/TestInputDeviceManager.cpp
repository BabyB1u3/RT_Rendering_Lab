#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "Core/Event/EventBus.h"
#include "Core/Event/Events.h"
#include "Core/Input/Device/InputDeviceManager.h"

namespace
{
    class FakeDevice final : public InputDevice
    {
    public:
        explicit FakeDevice(Type type, uint8_t deviceIndex = 0)
            : m_Type(type), m_DeviceIndex(deviceIndex) {}

        Type GetType() const override { return m_Type; }
        void Poll() override
        {
            ++PollCount;
            m_PreviousConnected = m_Connected;
            m_Connected = m_NextConnected;
            m_ConnectionChanged = m_HasPolled && (m_Connected != m_PreviousConnected);
            m_HasPolled = true;
        }

        InputValue GetInput(uint16_t) const override { return {}; }
        bool IsConnected() const override { return m_Connected; }
        bool WasConnected() const override { return m_PreviousConnected; }
        bool HasConnectionStateChanged() const override { return m_ConnectionChanged; }
        uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }
        void Reset() override { ++ResetCount; }

        void SetNextConnected(bool connected) { m_NextConnected = connected; }

        int PollCount = 0;
        int ResetCount = 0;

    private:
        Type m_Type;
        uint8_t m_DeviceIndex = 0;
        bool m_Connected = false;
        bool m_PreviousConnected = false;
        bool m_NextConnected = false;
        bool m_ConnectionChanged = false;
        bool m_HasPolled = false;
    };
}

TEST(InputDeviceManagerTests, GetDeviceReturnsLogicalSlotRegardlessOfInsertionOrder)
{
    InputDeviceManager manager;
    auto *keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto *mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto *gamepad2 = new FakeDevice(InputDevice::Type::Gamepad, 2);
    auto *gamepad0 = new FakeDevice(InputDevice::Type::Gamepad, 0);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad2));
    manager.AddDevice(Scope<InputDevice>(gamepad0));

    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Keyboard, 0), keyboard);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Mouse, 0), mouse);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 0), gamepad0);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 2), gamepad2);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 1), nullptr);
}

TEST(InputDeviceManagerTests, AddDeviceReplacesExistingDeviceInSameSlot)
{
    InputDeviceManager manager;
    auto *original = new FakeDevice(InputDevice::Type::Gamepad, 3);
    auto *replacement = new FakeDevice(InputDevice::Type::Gamepad, 3);

    manager.AddDevice(Scope<InputDevice>(original));
    manager.AddDevice(Scope<InputDevice>(replacement));

    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Gamepad), 1u);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 3), replacement);
}

TEST(InputDeviceManagerTests, AddDeviceIgnoresNullDevices)
{
    InputDeviceManager manager;

    manager.AddDevice(nullptr);

    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Keyboard), 0u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Mouse), 0u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Gamepad), 0u);
}

TEST(InputDeviceManagerTests, PollAllPollsEveryRegisteredDevice)
{
    InputDeviceManager manager;
    auto *keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto *mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto *gamepad = new FakeDevice(InputDevice::Type::Gamepad, 0);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    manager.PollAll();

    EXPECT_EQ(keyboard->PollCount, 1);
    EXPECT_EQ(mouse->PollCount, 1);
    EXPECT_EQ(gamepad->PollCount, 1);
}

TEST(InputDeviceManagerTests, ResetAllResetsEveryRegisteredDevice)
{
    InputDeviceManager manager;
    auto *keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto *mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto *gamepad = new FakeDevice(InputDevice::Type::Gamepad, 0);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    manager.ResetAll();

    EXPECT_EQ(keyboard->ResetCount, 1);
    EXPECT_EQ(mouse->ResetCount, 1);
    EXPECT_EQ(gamepad->ResetCount, 1);
}

TEST(InputDeviceManagerTests, GetDeviceCountCountsDevicesPerTypeAcrossSlots)
{
    InputDeviceManager manager;
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Keyboard)));
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Mouse)));
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Gamepad, 0)));
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Gamepad, 2)));

    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Keyboard), 1u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Mouse), 1u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Gamepad), 2u);
}

TEST(InputDeviceManagerTests, RemoveDeviceReturnsOwnedSlotAndClearsLookup)
{
    InputDeviceManager manager;
    auto *keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto *gamepad = new FakeDevice(InputDevice::Type::Gamepad, 1);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    auto removed = manager.RemoveDevice(InputDevice::Type::Gamepad, 1);

    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed.get(), gamepad);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 1), nullptr);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Keyboard, 0), keyboard);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Gamepad), 0u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Keyboard), 1u);
}

TEST(InputDeviceManagerTests, ClearRemovesAllDevices)
{
    InputDeviceManager manager;
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Keyboard)));
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Mouse)));
    manager.AddDevice(Scope<InputDevice>(new FakeDevice(InputDevice::Type::Gamepad, 0)));

    manager.Clear();

    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Keyboard, 0), nullptr);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Mouse, 0), nullptr);
    EXPECT_EQ(manager.GetDevice(InputDevice::Type::Gamepad, 0), nullptr);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Keyboard), 0u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Mouse), 0u);
    EXPECT_EQ(manager.GetDeviceCount(InputDevice::Type::Gamepad), 0u);
}

TEST(InputDeviceManagerTests, PollAllPublishesGamepadConnectionTransitions)
{
    InputDeviceManager manager;
    EventBus bus;
    std::vector<uint8_t> connected;
    std::vector<uint8_t> disconnected;

    auto connectedConnection = bus.Subscribe<GamepadConnectedEvent>(
        [&](const GamepadConnectedEvent &event) { connected.push_back(event.DeviceIndex); });
    auto disconnectedConnection = bus.Subscribe<GamepadDisconnectedEvent>(
        [&](const GamepadDisconnectedEvent &event) { disconnected.push_back(event.DeviceIndex); });

    auto *gamepad = new FakeDevice(InputDevice::Type::Gamepad, 2);
    manager.AddDevice(Scope<InputDevice>(gamepad));
    manager.SetEventBus(&bus);

    manager.PollAll();
    EXPECT_TRUE(connected.empty());
    EXPECT_TRUE(disconnected.empty());

    gamepad->SetNextConnected(true);
    manager.PollAll();
    ASSERT_EQ(connected.size(), 1u);
    EXPECT_EQ(connected[0], 2);

    gamepad->SetNextConnected(false);
    manager.PollAll();
    ASSERT_EQ(disconnected.size(), 1u);
    EXPECT_EQ(disconnected[0], 2);

    (void) connectedConnection;
    (void) disconnectedConnection;
}
