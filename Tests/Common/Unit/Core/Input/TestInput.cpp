#include <gtest/gtest.h>

#include "Core/Input/Input.h"
#include "Core/Input/Device/InputDeviceManager.h"
#include "InputTestAccess.h"

namespace
{
using test_support::InputTestAccess;

class FakeKeyboardDevice final : public InputDevice
{
public:
    explicit FakeKeyboardDevice(Key::Code activeKey) : m_ActiveKey(activeKey) {}

    Type GetType() const override { return Type::Keyboard; }
    void Poll() override {}

    InputValue GetInput(uint16_t code) const override { return {code == m_ActiveKey ? 1.0f : 0.0f, 0.0f}; }

    InputValue GetPreviousInput(uint16_t code) const override { return {code == m_PreviousKey ? 1.0f : 0.0f, 0.0f}; }

    void Reset() override { m_PreviousKey = 0; }

private:
    Key::Code m_ActiveKey = 0;
    Key::Code m_PreviousKey = 0;
};

class FakeGamepadDevice final : public InputDevice
{
public:
    explicit FakeGamepadDevice(uint8_t deviceIndex) : m_DeviceIndex(deviceIndex) {}

    Type GetType() const override { return Type::Gamepad; }
    void Poll() override {}

    InputValue GetInput(uint16_t code) const override { return {code == GamepadButton::A ? 1.0f : 0.0f, 0.0f}; }

    InputValue GetAxis(uint16_t axisId) const override { return {axisId == GamepadAxis::LeftX ? -0.5f : 0.0f, 0.0f}; }

    bool IsConnected() const override { return true; }
    uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }

private:
    uint8_t m_DeviceIndex = 0;
};

class FakeCustomDevice final : public InputDevice
{
public:
    explicit FakeCustomDevice(uint8_t deviceIndex) : m_DeviceIndex(deviceIndex) {}

    Type GetType() const override { return Type::Custom; }
    void Poll() override {}
    InputValue GetInput(uint16_t) const override { return {}; }
    uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }

private:
    uint8_t m_DeviceIndex = 0;
};

class InputPollingTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InputTestAccess::RestoreDefaultDevices();
        Input::Initialize(nullptr);
    }
};
} // namespace

TEST_F(InputPollingTests, InitializeResetsPollingState)
{
    auto frame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(frame, Key::W, true);
    InputTestAccess::SetMouseButton(frame, Mouse::Left, true);
    InputTestAccess::SetMousePosition(frame, 48.0f, 96.0f);
    Input::AccumulateScroll(2.5f);
    InputTestAccess::ApplyFrame(frame);
    Input::SetKeyboardCaptured(true);
    Input::SetMouseCaptured(true);

    Input::Initialize(nullptr);

    EXPECT_FALSE(Input::IsKeyDown(Key::W));
    EXPECT_FALSE(Input::IsMouseButtonDown(Mouse::Left));
    EXPECT_EQ(Input::GetMousePosition(), std::make_pair(0.0f, 0.0f));
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 0.0f);
    EXPECT_FALSE(Input::IsKeyboardCaptured());
    EXPECT_FALSE(Input::IsMouseCaptured());
}

TEST_F(InputPollingTests, InitializeWithSameNonNullWindowDoesNotResetState)
{
    auto* window = reinterpret_cast<GLFWwindow*>(0x1);
    Input::Initialize(window);

    auto frame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(frame, Key::W, true);
    InputTestAccess::SetMouseButton(frame, Mouse::Left, true);
    InputTestAccess::SetMousePosition(frame, 48.0f, 96.0f);
    Input::AccumulateScroll(2.5f);
    InputTestAccess::ApplyFrame(frame);
    Input::SetKeyboardCaptured(true);
    Input::SetMouseCaptured(true);

    Input::Initialize(window);

    EXPECT_FALSE(Input::IsKeyDown(Key::W));
    EXPECT_FALSE(Input::IsMouseButtonDown(Mouse::Left));
    EXPECT_EQ(Input::GetMousePosition(), std::make_pair(48.0f, 96.0f));
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 0.0f);
    EXPECT_TRUE(Input::IsKeyboardCaptured());
    EXPECT_TRUE(Input::IsMouseCaptured());

    Input::SetKeyboardCaptured(false);
    Input::SetMouseCaptured(false);
    EXPECT_TRUE(Input::IsKeyDown(Key::W));
    EXPECT_TRUE(Input::IsMouseButtonDown(Mouse::Left));
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 2.5f);
}

TEST_F(InputPollingTests, KeyboardPollingTracksPressHoldAndReleaseAcrossFrames)
{
    auto pressedFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(pressedFrame, Key::Space, true);

    InputTestAccess::ApplyFrame(pressedFrame);
    EXPECT_TRUE(Input::IsKeyDown(Key::Space));
    EXPECT_TRUE(Input::WasKeyPressedThisFrame(Key::Space));
    EXPECT_FALSE(Input::WasKeyReleasedThisFrame(Key::Space));

    InputTestAccess::ApplyFrame(pressedFrame);
    EXPECT_TRUE(Input::IsKeyDown(Key::Space));
    EXPECT_FALSE(Input::WasKeyPressedThisFrame(Key::Space));
    EXPECT_FALSE(Input::WasKeyReleasedThisFrame(Key::Space));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(Input::IsKeyDown(Key::Space));
    EXPECT_FALSE(Input::WasKeyPressedThisFrame(Key::Space));
    EXPECT_TRUE(Input::WasKeyReleasedThisFrame(Key::Space));
}

TEST_F(InputPollingTests, KeyboardCaptureBlocksQueriesButPreservesUnderlyingState)
{
    auto frame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(frame, Key::W, true);
    InputTestAccess::ApplyFrame(frame);

    Input::SetKeyboardCaptured(true);
    EXPECT_FALSE(Input::IsKeyDown(Key::W));
    EXPECT_FALSE(Input::WasKeyPressedThisFrame(Key::W));
    EXPECT_FALSE(Input::WasKeyReleasedThisFrame(Key::W));

    Input::SetKeyboardCaptured(false);
    EXPECT_TRUE(Input::IsKeyDown(Key::W));
    EXPECT_TRUE(Input::WasKeyPressedThisFrame(Key::W));
}

TEST_F(InputPollingTests, KeyboardQueriesReturnFalseForOutOfRangeCodes)
{
    constexpr Key::Code kOutOfRangeKey = static_cast<Key::Code>(600);

    EXPECT_FALSE(Input::IsKeyDown(kOutOfRangeKey));
    EXPECT_FALSE(Input::WasKeyPressedThisFrame(kOutOfRangeKey));
    EXPECT_FALSE(Input::WasKeyReleasedThisFrame(kOutOfRangeKey));
}

TEST_F(InputPollingTests, MouseButtonPollingTracksPressHoldAndReleaseAcrossFrames)
{
    auto pressedFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMouseButton(pressedFrame, Mouse::Right, true);

    InputTestAccess::ApplyFrame(pressedFrame);
    EXPECT_TRUE(Input::IsMouseButtonDown(Mouse::Right));
    EXPECT_TRUE(Input::WasMouseButtonPressedThisFrame(Mouse::Right));
    EXPECT_FALSE(Input::WasMouseButtonReleasedThisFrame(Mouse::Right));

    InputTestAccess::ApplyFrame(pressedFrame);
    EXPECT_TRUE(Input::IsMouseButtonDown(Mouse::Right));
    EXPECT_FALSE(Input::WasMouseButtonPressedThisFrame(Mouse::Right));
    EXPECT_FALSE(Input::WasMouseButtonReleasedThisFrame(Mouse::Right));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(Input::IsMouseButtonDown(Mouse::Right));
    EXPECT_FALSE(Input::WasMouseButtonPressedThisFrame(Mouse::Right));
    EXPECT_TRUE(Input::WasMouseButtonReleasedThisFrame(Mouse::Right));
}

TEST_F(InputPollingTests, MouseButtonQueriesReturnFalseForOutOfRangeCodes)
{
    constexpr Mouse::Code kOutOfRangeButton = static_cast<Mouse::Code>(32);

    EXPECT_FALSE(Input::IsMouseButtonDown(kOutOfRangeButton));
    EXPECT_FALSE(Input::WasMouseButtonPressedThisFrame(kOutOfRangeButton));
    EXPECT_FALSE(Input::WasMouseButtonReleasedThisFrame(kOutOfRangeButton));
}

TEST_F(InputPollingTests, MousePositionAndDeltaFollowFrameProgression)
{
    auto firstFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMousePosition(firstFrame, 100.0f, 50.0f);
    InputTestAccess::ApplyFrame(firstFrame);
    EXPECT_EQ(Input::GetMousePosition(), std::make_pair(100.0f, 50.0f));
    EXPECT_FLOAT_EQ(Input::GetMouseX(), 100.0f);
    EXPECT_FLOAT_EQ(Input::GetMouseY(), 50.0f);
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(0.0f, 0.0f));

    auto secondFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMousePosition(secondFrame, 112.0f, 44.0f);
    InputTestAccess::ApplyFrame(secondFrame);
    EXPECT_EQ(Input::GetMousePosition(), std::make_pair(112.0f, 44.0f));
    EXPECT_FLOAT_EQ(Input::GetMouseX(), 112.0f);
    EXPECT_FLOAT_EQ(Input::GetMouseY(), 44.0f);
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(12.0f, -6.0f));

    InputTestAccess::ApplyFrame(secondFrame);
    EXPECT_FLOAT_EQ(Input::GetMouseX(), 112.0f);
    EXPECT_FLOAT_EQ(Input::GetMouseY(), 44.0f);
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(0.0f, 0.0f));
}

TEST_F(InputPollingTests, MouseDevicePreviousDeltaTracksPriorFrameMotion)
{
    auto firstFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMousePosition(firstFrame, 10.0f, 20.0f);
    InputTestAccess::ApplyFrame(firstFrame);

    auto secondFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMousePosition(secondFrame, 24.0f, 18.0f);
    InputTestAccess::ApplyFrame(secondFrame);

    auto thirdFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMousePosition(thirdFrame, 30.0f, 30.0f);
    InputTestAccess::ApplyFrame(thirdFrame);

    const auto* manager = Input::TryGetDeviceManager();
    ASSERT_NE(manager, nullptr);

    const auto* mouse = manager->GetDevice(InputDevice::Type::Mouse);
    ASSERT_NE(mouse, nullptr);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxisId::DeltaX).X, 14.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxisId::DeltaY).X, -2.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaX).X, 6.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaY).X, 12.0f);
}

TEST_F(InputPollingTests, MouseCaptureBlocksButtonDeltaAndScrollQueries)
{
    auto frame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMouseButton(frame, Mouse::Left, true);
    InputTestAccess::SetMousePosition(frame, 25.0f, 40.0f);
    InputTestAccess::ApplyFrame(frame);

    auto movedFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetMouseButton(movedFrame, Mouse::Left, true);
    InputTestAccess::SetMousePosition(movedFrame, 40.0f, 70.0f);
    Input::AccumulateScroll(1.5f);
    InputTestAccess::ApplyFrame(movedFrame);

    Input::SetMouseCaptured(true);
    EXPECT_FALSE(Input::IsMouseButtonDown(Mouse::Left));
    EXPECT_FALSE(Input::WasMouseButtonPressedThisFrame(Mouse::Left));
    EXPECT_FALSE(Input::WasMouseButtonReleasedThisFrame(Mouse::Left));
    EXPECT_EQ(Input::GetMousePosition(), std::make_pair(40.0f, 70.0f));
    EXPECT_FLOAT_EQ(Input::GetMouseX(), 40.0f);
    EXPECT_FLOAT_EQ(Input::GetMouseY(), 70.0f);
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 0.0f);

    Input::SetMouseCaptured(false);
    EXPECT_TRUE(Input::IsMouseButtonDown(Mouse::Left));
    EXPECT_EQ(Input::GetMouseDelta(), std::make_pair(15.0f, 30.0f));
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 1.5f);
}

TEST_F(InputPollingTests, ScrollDeltaConsumesAccumulatorPerFrame)
{
    Input::AccumulateScroll(1.0f);
    Input::AccumulateScroll(-0.25f);
    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 0.75f);

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FLOAT_EQ(Input::GetScrollDelta(), 0.0f);
}

TEST_F(InputPollingTests, GamepadButtonPollingTracksPressHoldAndReleaseAcrossFrames)
{
    auto pressedFrame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::SetGamepadButton(pressedFrame, GamepadButton::A, true);

    InputTestAccess::ApplyGamepadFrame(0, pressedFrame);
    EXPECT_TRUE(Input::IsGamepadConnected());
    EXPECT_TRUE(Input::IsGamepadButtonDown(GamepadButton::A));
    EXPECT_TRUE(Input::WasGamepadButtonPressedThisFrame(GamepadButton::A));
    EXPECT_FALSE(Input::WasGamepadButtonReleasedThisFrame(GamepadButton::A));

    InputTestAccess::ApplyGamepadFrame(0, pressedFrame);
    EXPECT_TRUE(Input::IsGamepadButtonDown(GamepadButton::A));
    EXPECT_FALSE(Input::WasGamepadButtonPressedThisFrame(GamepadButton::A));
    EXPECT_FALSE(Input::WasGamepadButtonReleasedThisFrame(GamepadButton::A));

    InputTestAccess::ApplyGamepadFrame(0, InputTestAccess::MakeGamepadFrame(true));
    EXPECT_FALSE(Input::IsGamepadButtonDown(GamepadButton::A));
    EXPECT_FALSE(Input::WasGamepadButtonPressedThisFrame(GamepadButton::A));
    EXPECT_TRUE(Input::WasGamepadButtonReleasedThisFrame(GamepadButton::A));
}

TEST_F(InputPollingTests, GamepadAxisReturnsCurrentValue)
{
    auto frame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::SetGamepadAxis(frame, GamepadAxis::LeftX, 0.75f);
    InputTestAccess::SetGamepadAxis(frame, GamepadAxis::RightTrigger, 1.0f);

    InputTestAccess::ApplyGamepadFrame(0, frame);

    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(GamepadAxis::LeftX), 0.75f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(GamepadAxis::RightTrigger), 1.0f);
}

TEST_F(InputPollingTests, RegisterDeviceReplacesKeyboardSlotForFacadeQueries)
{
    Input::RegisterDevice(CreateScope<FakeKeyboardDevice>(Key::Q));

    EXPECT_TRUE(Input::IsKeyDown(Key::Q));
    EXPECT_FALSE(Input::IsKeyDown(Key::W));
}

TEST_F(InputPollingTests, RegisterDeviceUsesLogicalGamepadSlotIndex)
{
    Input::RegisterDevice(CreateScope<FakeGamepadDevice>(2));

    EXPECT_TRUE(Input::IsGamepadConnected(2));
    EXPECT_TRUE(Input::IsGamepadButtonDown(GamepadButton::A, 2));
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(GamepadAxis::LeftX, 2), -0.5f);
    EXPECT_FALSE(Input::IsGamepadConnected(1));
}

TEST_F(InputPollingTests, RestoreDefaultDevicesReinstallsBuiltInPollingDevices)
{
    Input::RegisterDevice(CreateScope<FakeKeyboardDevice>(Key::Q));
    ASSERT_TRUE(Input::IsKeyDown(Key::Q));

    Input::RestoreDefaultDevices();

    auto frame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(frame, Key::W, true);
    InputTestAccess::ApplyFrame(frame);

    EXPECT_TRUE(Input::IsKeyDown(Key::W));
    EXPECT_FALSE(Input::IsKeyDown(Key::Q));
}

TEST_F(InputPollingTests, RestoreDefaultDevicesRemovesExtraDevicesOutsideDefaultLayout)
{
    Input::RegisterDevice(CreateScope<FakeGamepadDevice>(7));
    Input::RegisterDevice(CreateScope<FakeCustomDevice>(0));

    auto& managerBefore = Input::GetDeviceManager();
    ASSERT_NE(managerBefore.GetDevice(InputDevice::Type::Gamepad, 7), nullptr);
    ASSERT_NE(managerBefore.GetDevice(InputDevice::Type::Custom, 0), nullptr);

    Input::RestoreDefaultDevices();

    auto& managerAfter = Input::GetDeviceManager();
    EXPECT_EQ(managerAfter.GetDevice(InputDevice::Type::Gamepad, 7), nullptr);
    EXPECT_EQ(managerAfter.GetDevice(InputDevice::Type::Custom, 0), nullptr);
    EXPECT_NE(managerAfter.GetDevice(InputDevice::Type::Keyboard, 0), nullptr);
    EXPECT_NE(managerAfter.GetDevice(InputDevice::Type::Mouse, 0), nullptr);
    EXPECT_NE(managerAfter.GetDevice(InputDevice::Type::Gamepad, 0), nullptr);
    EXPECT_NE(managerAfter.GetDevice(InputDevice::Type::Gamepad, 3), nullptr);
}
