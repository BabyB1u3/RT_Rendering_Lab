#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Code/KeyCode.h"
#include "Core/Input/Code/MouseCode.h"
#include "Core/Input/Device/InputDeviceManager.h"
#include "Core/Input/Replay/InputRecording.h"
#include "Core/Input/Replay/ReplayDevice.h"
#include "TestPaths.h"

namespace
{
class ScriptedKeyboardDevice final : public InputDevice
{
public:
    using Frame = std::array<uint8_t, KeyboardDevice::k_KeyStateSize>;

    Type GetType() const override { return Type::Keyboard; }

    void Poll() override
    {
        m_Previous = m_Current;
        if (m_NextFrame < m_Frames.size())
            m_Current = m_Frames[m_NextFrame++];
        else
            m_Current.fill(0);
    }

    InputValue GetInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Current.size())
            return {};

        return {m_Current[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    InputValue GetPreviousInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Previous.size())
            return {};

        return {m_Previous[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    void PushFrame(const Frame& frame) { m_Frames.push_back(frame); }

private:
    std::vector<Frame> m_Frames;
    Frame m_Current{};
    Frame m_Previous{};
    std::size_t m_NextFrame = 0;
};

class ScriptedMouseDevice final : public InputDevice
{
public:
    struct Frame
    {
        std::array<uint8_t, MouseDevice::k_ButtonCount> buttons{};
        float x = 0.0f;
        float y = 0.0f;
        float scrollDelta = 0.0f;
        bool isConnected = true;
    };

    Type GetType() const override { return Type::Mouse; }

    void Poll() override
    {
        m_Previous = m_Current;
        if (m_NextFrame < m_Frames.size())
            m_Current = m_Frames[m_NextFrame++];
        else
            m_Current = {};
    }

    InputValue GetInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Current.buttons.size())
            return {};

        return {m_Current.buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    InputValue GetPreviousInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Previous.buttons.size())
            return {};

        return {m_Previous.buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    InputValue GetAxis(uint16_t axisId) const override
    {
        switch (axisId)
        {
            case MouseAxis::PositionX:
                return {m_Current.x, 0.0f};
            case MouseAxis::PositionY:
                return {m_Current.y, 0.0f};
            case MouseAxis::DeltaX:
                return {m_Current.x - m_Previous.x, 0.0f};
            case MouseAxis::DeltaY:
                return {m_Current.y - m_Previous.y, 0.0f};
            case MouseAxis::ScrollY:
                return {m_Current.scrollDelta, 0.0f};
            default:
                return {};
        }
    }

    InputValue GetPreviousAxis(uint16_t axisId) const override
    {
        switch (axisId)
        {
            case MouseAxis::PositionX:
                return {m_Previous.x, 0.0f};
            case MouseAxis::PositionY:
                return {m_Previous.y, 0.0f};
            default:
                return {};
        }
    }

    bool IsConnected() const override { return m_Current.isConnected; }
    bool WasConnected() const override { return m_Previous.isConnected; }

    void PushFrame(const Frame& frame) { m_Frames.push_back(frame); }

private:
    std::vector<Frame> m_Frames;
    Frame m_Current{};
    Frame m_Previous{};
    std::size_t m_NextFrame = 0;
};

class ScriptedGamepadDevice final : public InputDevice
{
public:
    struct Frame
    {
        bool isConnected = false;
        std::array<uint8_t, GamepadButton::Count> buttons{};
        std::array<float, GamepadAxis::Count> axes{};
    };

    explicit ScriptedGamepadDevice(uint8_t deviceIndex) : m_DeviceIndex(deviceIndex) {}

    Type GetType() const override { return Type::Gamepad; }

    void Poll() override
    {
        m_Previous = m_Current;
        if (m_NextFrame < m_Frames.size())
            m_Current = m_Frames[m_NextFrame++];
        else
            m_Current = {};
    }

    InputValue GetInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Current.buttons.size())
            return {};

        return {m_Current.buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    InputValue GetPreviousInput(uint16_t code) const override
    {
        const auto index = static_cast<std::size_t>(code);
        if (index >= m_Previous.buttons.size())
            return {};

        return {m_Previous.buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
    }

    InputValue GetAxis(uint16_t axisId) const override
    {
        const auto index = static_cast<std::size_t>(axisId);
        if (index >= m_Current.axes.size())
            return {};

        return {m_Current.axes[index], 0.0f};
    }

    InputValue GetPreviousAxis(uint16_t axisId) const override
    {
        const auto index = static_cast<std::size_t>(axisId);
        if (index >= m_Previous.axes.size())
            return {};

        return {m_Previous.axes[index], 0.0f};
    }

    bool IsConnected() const override { return m_Current.isConnected; }
    bool WasConnected() const override { return m_Previous.isConnected; }
    uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }

    void PushFrame(const Frame& frame) { m_Frames.push_back(frame); }

private:
    uint8_t m_DeviceIndex = 0;
    std::vector<Frame> m_Frames;
    Frame m_Current{};
    Frame m_Previous{};
    std::size_t m_NextFrame = 0;
};

class InputRecordingTests : public ::testing::Test
{
protected:
    void SetUp() override { test_support::ResetCurrentTestRoot("input-recording"); }

    void TearDown() override { test_support::RemoveCurrentTestArtifacts("input-recording"); }
};
} // namespace

TEST_F(InputRecordingTests, RecorderCapturesPolledFramesAcrossDefaultDeviceTypes)
{
    InputDeviceManager manager;
    InputRecorder recorder;

    auto* keyboard = new ScriptedKeyboardDevice();
    ScriptedKeyboardDevice::Frame keyboardFrame0{};
    keyboardFrame0[static_cast<std::size_t>(Key::W)] = 1u;
    keyboard->PushFrame(keyboardFrame0);

    ScriptedKeyboardDevice::Frame keyboardFrame1{};
    keyboardFrame1[static_cast<std::size_t>(Key::Space)] = 1u;
    keyboard->PushFrame(keyboardFrame1);

    auto* mouse = new ScriptedMouseDevice();
    ScriptedMouseDevice::Frame mouseFrame0;
    mouseFrame0.buttons[Mouse::Left] = 1;
    mouseFrame0.x = 10.0f;
    mouseFrame0.y = 20.0f;
    mouseFrame0.scrollDelta = 1.5f;
    mouse->PushFrame(mouseFrame0);

    ScriptedMouseDevice::Frame mouseFrame1;
    mouseFrame1.buttons[Mouse::Right] = 1;
    mouseFrame1.x = 15.0f;
    mouseFrame1.y = 5.0f;
    mouseFrame1.scrollDelta = -0.5f;
    mouse->PushFrame(mouseFrame1);

    auto* gamepad = new ScriptedGamepadDevice(1);
    ScriptedGamepadDevice::Frame gamepadFrame0;
    gamepadFrame0.isConnected = true;
    gamepadFrame0.buttons[GamepadButton::A] = 1;
    gamepadFrame0.axes[GamepadAxis::LeftX] = 0.25f;
    gamepad->PushFrame(gamepadFrame0);

    ScriptedGamepadDevice::Frame gamepadFrame1;
    gamepadFrame1.isConnected = true;
    gamepadFrame1.buttons[GamepadButton::B] = 1;
    gamepadFrame1.axes[GamepadAxis::RightY] = -0.75f;
    gamepad->PushFrame(gamepadFrame1);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    recorder.Attach(manager);
    recorder.BeginRecording();

    manager.PollAll(0.016f);
    manager.PollAll(0.033f);
    recorder.EndRecording();

    const auto& recording = recorder.GetRecording();
    ASSERT_EQ(recording.frames.size(), 2u);

    const auto& frame0 = recording.frames[0];
    EXPECT_EQ(frame0.frameNumber, 0u);
    EXPECT_FLOAT_EQ(frame0.deltaTime, 0.016f);
    EXPECT_TRUE(frame0.keyboard.isPresent);
    EXPECT_EQ(frame0.keyboard.keys[Key::W], 1u);
    EXPECT_EQ(frame0.keyboard.keys[Key::Space], 0u);
    EXPECT_TRUE(frame0.mouse.isPresent);
    EXPECT_EQ(frame0.mouse.buttons[Mouse::Left], 1u);
    EXPECT_FLOAT_EQ(frame0.mouse.x, 10.0f);
    EXPECT_FLOAT_EQ(frame0.mouse.y, 20.0f);
    EXPECT_FLOAT_EQ(frame0.mouse.scrollDelta, 1.5f);
    ASSERT_EQ(frame0.gamepads.size(), 1u);
    EXPECT_EQ(frame0.gamepads[0].deviceIndex, 1);
    EXPECT_TRUE(frame0.gamepads[0].isConnected);
    EXPECT_EQ(frame0.gamepads[0].buttons[GamepadButton::A], 1u);
    EXPECT_FLOAT_EQ(frame0.gamepads[0].axes[GamepadAxis::LeftX], 0.25f);

    const auto& frame1 = recording.frames[1];
    EXPECT_EQ(frame1.frameNumber, 1u);
    EXPECT_FLOAT_EQ(frame1.deltaTime, 0.033f);
    EXPECT_EQ(frame1.keyboard.keys[Key::W], 0u);
    EXPECT_EQ(frame1.keyboard.keys[Key::Space], 1u);
    EXPECT_EQ(frame1.mouse.buttons[Mouse::Right], 1u);
    EXPECT_FLOAT_EQ(frame1.mouse.x, 15.0f);
    EXPECT_FLOAT_EQ(frame1.mouse.y, 5.0f);
    EXPECT_FLOAT_EQ(frame1.mouse.scrollDelta, -0.5f);
    ASSERT_EQ(frame1.gamepads.size(), 1u);
    EXPECT_EQ(frame1.gamepads[0].buttons[GamepadButton::B], 1u);
    EXPECT_FLOAT_EQ(frame1.gamepads[0].axes[GamepadAxis::RightY], -0.75f);
}

TEST_F(InputRecordingTests, ReplayDevicesExposeCurrentAndPreviousRecordedState)
{
    InputRecording recording;

    InputFrame frame0;
    frame0.frameNumber = 0;
    frame0.deltaTime = 0.016f;
    frame0.keyboard.isPresent = true;
    frame0.keyboard.isConnected = true;
    frame0.keyboard.keys[Key::Space] = 1;
    frame0.mouse.isPresent = true;
    frame0.mouse.isConnected = true;
    frame0.mouse.buttons[Mouse::Left] = 1;
    frame0.mouse.x = 100.0f;
    frame0.mouse.y = 50.0f;
    frame0.mouse.scrollDelta = 1.0f;
    RecordedGamepadState gamepad0;
    gamepad0.deviceIndex = 1;
    gamepad0.isConnected = true;
    gamepad0.buttons[GamepadButton::A] = 1;
    gamepad0.axes[GamepadAxis::LeftX] = 0.5f;
    frame0.gamepads.push_back(gamepad0);
    recording.frames.push_back(frame0);

    InputFrame frame1;
    frame1.frameNumber = 1;
    frame1.deltaTime = 0.033f;
    frame1.keyboard.isPresent = true;
    frame1.keyboard.isConnected = true;
    frame1.mouse.isPresent = true;
    frame1.mouse.isConnected = true;
    frame1.mouse.buttons[Mouse::Right] = 1;
    frame1.mouse.x = 110.0f;
    frame1.mouse.y = 40.0f;
    frame1.mouse.scrollDelta = 0.0f;
    RecordedGamepadState gamepad1;
    gamepad1.deviceIndex = 1;
    gamepad1.isConnected = false;
    frame1.gamepads.push_back(gamepad1);
    recording.frames.push_back(frame1);

    InputReplaySession session;
    session.SetRecording(std::move(recording));

    InputDeviceManager manager;
    session.Attach(manager);
    manager.AddDevice(CreateScope<ReplayKeyboardDevice>(session));
    manager.AddDevice(CreateScope<ReplayMouseDevice>(session));
    manager.AddDevice(CreateScope<ReplayGamepadDevice>(session, 1));

    manager.PollAll(0.016f);
    auto* keyboard = manager.GetDevice(InputDevice::Type::Keyboard);
    auto* mouse = manager.GetDevice(InputDevice::Type::Mouse);
    auto* gamepad = manager.GetDevice(InputDevice::Type::Gamepad, 1);
    ASSERT_NE(keyboard, nullptr);
    ASSERT_NE(mouse, nullptr);
    ASSERT_NE(gamepad, nullptr);

    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).x, 1.0f);
    EXPECT_FLOAT_EQ(keyboard->GetPreviousInput(Key::Space).x, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Left).x, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::PositionX).x, 100.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::DeltaX).x, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::ScrollY).x, 1.0f);
    EXPECT_TRUE(gamepad->IsConnected());
    EXPECT_FLOAT_EQ(gamepad->GetInput(GamepadButton::A).x, 1.0f);
    EXPECT_FLOAT_EQ(gamepad->GetAxis(GamepadAxis::LeftX).x, 0.5f);

    manager.PollAll(0.033f);
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).x, 0.0f);
    EXPECT_FLOAT_EQ(keyboard->GetPreviousInput(Key::Space).x, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Left).x, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Right).x, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousInput(Mouse::Left).x, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::PositionX).x, 110.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::PositionY).x, 40.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::DeltaX).x, 10.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::DeltaY).x, -10.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxis::ScrollY).x, 1.0f);
    EXPECT_FALSE(gamepad->IsConnected());
    EXPECT_TRUE(gamepad->WasConnected());
    EXPECT_FLOAT_EQ(gamepad->GetInput(GamepadButton::A).x, 0.0f);
    EXPECT_FLOAT_EQ(gamepad->GetPreviousInput(GamepadButton::A).x, 1.0f);

    manager.PollAll(0.05f);
    EXPECT_TRUE(session.IsFinished());
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).x, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::PositionX).x, 110.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxis::DeltaX).x, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxis::DeltaX).x, 10.0f);
    EXPECT_FALSE(gamepad->IsConnected());
}

TEST_F(InputRecordingTests, ReplaySessionFlagsDeltaTimeMismatchWithoutStoppingPlayback)
{
    InputRecording recording;
    InputFrame frame;
    frame.frameNumber = 0;
    frame.deltaTime = 0.01f;
    frame.keyboard.isPresent = true;
    frame.keyboard.isConnected = true;
    frame.keyboard.keys[Key::Tab] = 1;
    recording.frames.push_back(frame);

    InputReplaySession session;
    session.SetRecording(std::move(recording));

    InputDeviceManager manager;
    session.Attach(manager);
    manager.AddDevice(CreateScope<ReplayKeyboardDevice>(session));

    manager.PollAll(0.03f);

    auto* keyboard = manager.GetDevice(InputDevice::Type::Keyboard);
    ASSERT_NE(keyboard, nullptr);
    EXPECT_TRUE(session.HasTimingMismatch());
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Tab).x, 1.0f);
}
