#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "Core/Event/EventBus.h"
#include "Core/Event/Events.h"
#include "Core/Input/Device/InputDeviceManager.h"

namespace
{
class FakeDevice final : public InputDevice
{
public:
    explicit FakeDevice(Type type, uint8_t deviceIndex = 0) : m_Type(type), m_DeviceIndex(deviceIndex) {}

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

class PollObserver final : public InputDeviceManagerObserver
{
public:
    void OnBeforePollAll(InputDeviceManager&, float dt) override { Events.push_back("before:" + std::to_string(dt)); }

    void OnAfterPollAll(const InputDeviceManager&, float dt) override
    {
        Events.push_back("after:" + std::to_string(dt));
    }

    std::vector<std::string> Events;
};

class TracedDevice final : public InputDevice
{
public:
    explicit TracedDevice(std::vector<std::string>& events) : m_Events(events) {}

    Type GetType() const override { return Type::Keyboard; }

    void Poll() override { m_Events.push_back("poll"); }

    InputValue GetInput(uint16_t) const override { return {}; }

private:
    std::vector<std::string>& m_Events;
};
} // namespace

TEST(InputDeviceManagerTests, GetDeviceReturnsLogicalSlotRegardlessOfInsertionOrder)
{
    InputDeviceManager manager;
    auto* keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto* mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto* gamepad2 = new FakeDevice(InputDevice::Type::Gamepad, 2);
    auto* gamepad0 = new FakeDevice(InputDevice::Type::Gamepad, 0);

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
    auto* original = new FakeDevice(InputDevice::Type::Gamepad, 3);
    auto* replacement = new FakeDevice(InputDevice::Type::Gamepad, 3);

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
    auto* keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto* mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto* gamepad = new FakeDevice(InputDevice::Type::Gamepad, 0);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    manager.PollAll();

    EXPECT_EQ(keyboard->PollCount, 1);
    EXPECT_EQ(mouse->PollCount, 1);
    EXPECT_EQ(gamepad->PollCount, 1);
}

TEST(InputDeviceManagerTests, PollObserversWrapDevicePollingInOrder)
{
    InputDeviceManager manager;
    PollObserver observer;

    manager.AddObserver(&observer);
    manager.AddDevice(Scope<InputDevice>(new TracedDevice(observer.Events)));

    manager.PollAll(0.25f);

    EXPECT_EQ(observer.Events, (std::vector<std::string>{"before:0.250000", "poll", "after:0.250000"}));
}

TEST(InputDeviceManagerTests, ResetAllResetsEveryRegisteredDevice)
{
    InputDeviceManager manager;
    auto* keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto* mouse = new FakeDevice(InputDevice::Type::Mouse);
    auto* gamepad = new FakeDevice(InputDevice::Type::Gamepad, 0);

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
    auto* keyboard = new FakeDevice(InputDevice::Type::Keyboard);
    auto* gamepad = new FakeDevice(InputDevice::Type::Gamepad, 1);

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
    std::vector<bool> connectionStates;

    auto connectedConnection = bus.Subscribe<GamepadConnectedEvent>([&](const GamepadConnectedEvent& event)
                                                                    { connected.push_back(event.m_DeviceIndex); });
    auto disconnectedConnection = bus.Subscribe<GamepadDisconnectedEvent>(
        [&](const GamepadDisconnectedEvent& event) { disconnected.push_back(event.m_DeviceIndex); });
    auto connectionChangedConnection = bus.Subscribe<DeviceConnectionChangedEvent>(
        [&](const DeviceConnectionChangedEvent& event)
        {
            if (event.m_DeviceType == InputDevice::Type::Gamepad && event.m_DeviceIndex == 2)
                connectionStates.push_back(event.m_IsConnected);
        });

    auto* gamepad = new FakeDevice(InputDevice::Type::Gamepad, 2);
    manager.AddDevice(Scope<InputDevice>(gamepad));
    manager.SetEventBus(&bus);

    manager.PollAll();
    EXPECT_TRUE(connected.empty());
    EXPECT_TRUE(disconnected.empty());

    gamepad->SetNextConnected(true);
    manager.PollAll();
    ASSERT_EQ(connected.size(), 1u);
    EXPECT_EQ(connected[0], 2);
    ASSERT_EQ(connectionStates.size(), 1u);
    EXPECT_TRUE(connectionStates[0]);

    gamepad->SetNextConnected(false);
    manager.PollAll();
    ASSERT_EQ(disconnected.size(), 1u);
    EXPECT_EQ(disconnected[0], 2);
    ASSERT_EQ(connectionStates.size(), 2u);
    EXPECT_FALSE(connectionStates[1]);

    (void)connectedConnection;
    (void)disconnectedConnection;
    (void)connectionChangedConnection;
}

TEST(InputDeviceManagerTests, AddRemoveAndClearPublishDeviceLifecycleEvents)
{
    InputDeviceManager manager;
    EventBus bus;
    std::vector<std::string> events;

    auto attachedConnection = bus.Subscribe<DeviceAttachedToSlotEvent>(
        [&](const DeviceAttachedToSlotEvent& event)
        {
            events.push_back("attach:" + std::to_string(static_cast<int>(event.m_DeviceType)) + ":" +
                             std::to_string(event.m_DeviceIndex));
        });
    auto detachedConnection = bus.Subscribe<DeviceDetachedFromSlotEvent>(
        [&](const DeviceDetachedFromSlotEvent& event)
        {
            events.push_back("detach:" + std::to_string(static_cast<int>(event.m_DeviceType)) + ":" +
                             std::to_string(event.m_DeviceIndex));
        });
    auto connectionChangedConnection = bus.Subscribe<DeviceConnectionChangedEvent>(
        [&](const DeviceConnectionChangedEvent& event)
        {
            events.push_back("conn:" + std::to_string(static_cast<int>(event.m_DeviceType)) + ":" +
                             std::to_string(event.m_DeviceIndex) + ":" + (event.m_IsConnected ? "1" : "0"));
        });
    auto gamepadConnectedConnection = bus.Subscribe<GamepadConnectedEvent>(
        [&](const GamepadConnectedEvent& event) { events.push_back("gconn:" + std::to_string(event.m_DeviceIndex)); });
    auto gamepadDisconnectedConnection =
        bus.Subscribe<GamepadDisconnectedEvent>([&](const GamepadDisconnectedEvent& event)
                                                { events.push_back("gdisc:" + std::to_string(event.m_DeviceIndex)); });

    manager.SetEventBus(&bus);

    auto* gamepad = new FakeDevice(InputDevice::Type::Gamepad, 1);
    gamepad->SetNextConnected(true);
    gamepad->Poll();
    manager.AddDevice(Scope<InputDevice>(gamepad));

    EXPECT_EQ(events, (std::vector<std::string>{"attach:2:1", "conn:2:1:1", "gconn:1"}));

    events.clear();
    auto* replacement = new FakeDevice(InputDevice::Type::Gamepad, 1);
    replacement->SetNextConnected(true);
    replacement->Poll();
    manager.AddDevice(Scope<InputDevice>(replacement));

    EXPECT_EQ(events, (std::vector<std::string>{"detach:2:1", "attach:2:1"}));

    events.clear();
    auto removed = manager.RemoveDevice(InputDevice::Type::Gamepad, 1);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(events, (std::vector<std::string>{"detach:2:1", "conn:2:1:0", "gdisc:1"}));

    events.clear();
    auto* keyboard = new FakeDevice(InputDevice::Type::Keyboard, 0);
    keyboard->SetNextConnected(true);
    keyboard->Poll();
    auto* mouse = new FakeDevice(InputDevice::Type::Mouse, 0);
    mouse->SetNextConnected(true);
    mouse->Poll();
    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    events.clear();

    manager.Clear();

    EXPECT_EQ(events, (std::vector<std::string>{"detach:0:0", "conn:0:0:0", "detach:1:0", "conn:1:0:0"}));

    (void)attachedConnection;
    (void)detachedConnection;
    (void)connectionChangedConnection;
    (void)gamepadConnectedConnection;
    (void)gamepadDisconnectedConnection;
}

TEST(InputDeviceManagerTests, ReplacingDevicePublishesConnectionDeltaWhenSlotAvailabilityChanges)
{
    InputDeviceManager manager;
    EventBus bus;
    std::vector<std::string> events;

    auto attachedConnection = bus.Subscribe<DeviceAttachedToSlotEvent>(
        [&](const DeviceAttachedToSlotEvent& event)
        {
            if (event.m_DeviceType == InputDevice::Type::Gamepad && event.m_DeviceIndex == 3)
                events.push_back("attach");
        });
    auto detachedConnection = bus.Subscribe<DeviceDetachedFromSlotEvent>(
        [&](const DeviceDetachedFromSlotEvent& event)
        {
            if (event.m_DeviceType == InputDevice::Type::Gamepad && event.m_DeviceIndex == 3)
                events.push_back("detach");
        });
    auto connectionChangedConnection = bus.Subscribe<DeviceConnectionChangedEvent>(
        [&](const DeviceConnectionChangedEvent& event)
        {
            if (event.m_DeviceType == InputDevice::Type::Gamepad && event.m_DeviceIndex == 3)
                events.push_back(event.m_IsConnected ? "conn1" : "conn0");
        });
    auto gamepadConnectedConnection = bus.Subscribe<GamepadConnectedEvent>(
        [&](const GamepadConnectedEvent& event)
        {
            if (event.m_DeviceIndex == 3)
                events.push_back("gconn");
        });
    auto gamepadDisconnectedConnection = bus.Subscribe<GamepadDisconnectedEvent>(
        [&](const GamepadDisconnectedEvent& event)
        {
            if (event.m_DeviceIndex == 3)
                events.push_back("gdisc");
        });

    manager.SetEventBus(&bus);

    auto* disconnected = new FakeDevice(InputDevice::Type::Gamepad, 3);
    manager.AddDevice(Scope<InputDevice>(disconnected));
    EXPECT_EQ(events, (std::vector<std::string>{"attach"}));

    events.clear();
    auto* connected = new FakeDevice(InputDevice::Type::Gamepad, 3);
    connected->SetNextConnected(true);
    connected->Poll();
    manager.AddDevice(Scope<InputDevice>(connected));
    EXPECT_EQ(events, (std::vector<std::string>{"detach", "attach", "conn1", "gconn"}));

    events.clear();
    auto* replacementDisconnected = new FakeDevice(InputDevice::Type::Gamepad, 3);
    manager.AddDevice(Scope<InputDevice>(replacementDisconnected));
    EXPECT_EQ(events, (std::vector<std::string>{"detach", "attach", "conn0", "gdisc"}));

    (void)attachedConnection;
    (void)detachedConnection;
    (void)connectionChangedConnection;
    (void)gamepadConnectedConnection;
    (void)gamepadDisconnectedConnection;
}
