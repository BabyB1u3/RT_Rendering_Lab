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
        using Frame = std::array<uint8_t, KeyboardDevice::KEY_STATE_SIZE>;

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

        void PushFrame(const Frame &frame)
        {
            m_Frames.push_back(frame);
        }

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
            std::array<uint8_t, MouseDevice::BUTTON_COUNT> Buttons{};
            float X = 0.0f;
            float Y = 0.0f;
            float ScrollDelta = 0.0f;
            bool Connected = true;
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
            if (index >= m_Current.Buttons.size())
                return {};

            return {m_Current.Buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
        }

        InputValue GetPreviousInput(uint16_t code) const override
        {
            const auto index = static_cast<std::size_t>(code);
            if (index >= m_Previous.Buttons.size())
                return {};

            return {m_Previous.Buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
        }

        InputValue GetAxis(uint16_t axisId) const override
        {
            switch (axisId)
            {
            case MouseAxisId::PositionX:
                return {m_Current.X, 0.0f};
            case MouseAxisId::PositionY:
                return {m_Current.Y, 0.0f};
            case MouseAxisId::DeltaX:
                return {m_Current.X - m_Previous.X, 0.0f};
            case MouseAxisId::DeltaY:
                return {m_Current.Y - m_Previous.Y, 0.0f};
            case MouseAxisId::ScrollY:
                return {m_Current.ScrollDelta, 0.0f};
            default:
                return {};
            }
        }

        InputValue GetPreviousAxis(uint16_t axisId) const override
        {
            switch (axisId)
            {
            case MouseAxisId::PositionX:
                return {m_Previous.X, 0.0f};
            case MouseAxisId::PositionY:
                return {m_Previous.Y, 0.0f};
            default:
                return {};
            }
        }

        bool IsConnected() const override { return m_Current.Connected; }
        bool WasConnected() const override { return m_Previous.Connected; }

        void PushFrame(const Frame &frame)
        {
            m_Frames.push_back(frame);
        }

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
            bool Connected = false;
            std::array<uint8_t, GamepadButton::Count> Buttons{};
            std::array<float, GamepadAxis::Count> Axes{};
        };

        explicit ScriptedGamepadDevice(uint8_t deviceIndex)
            : m_DeviceIndex(deviceIndex) {}

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
            if (index >= m_Current.Buttons.size())
                return {};

            return {m_Current.Buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
        }

        InputValue GetPreviousInput(uint16_t code) const override
        {
            const auto index = static_cast<std::size_t>(code);
            if (index >= m_Previous.Buttons.size())
                return {};

            return {m_Previous.Buttons[index] != 0 ? 1.0f : 0.0f, 0.0f};
        }

        InputValue GetAxis(uint16_t axisId) const override
        {
            const auto index = static_cast<std::size_t>(axisId);
            if (index >= m_Current.Axes.size())
                return {};

            return {m_Current.Axes[index], 0.0f};
        }

        InputValue GetPreviousAxis(uint16_t axisId) const override
        {
            const auto index = static_cast<std::size_t>(axisId);
            if (index >= m_Previous.Axes.size())
                return {};

            return {m_Previous.Axes[index], 0.0f};
        }

        bool IsConnected() const override { return m_Current.Connected; }
        bool WasConnected() const override { return m_Previous.Connected; }
        uint8_t GetDeviceIndex() const override { return m_DeviceIndex; }

        void PushFrame(const Frame &frame)
        {
            m_Frames.push_back(frame);
        }

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
        void SetUp() override
        {
            test_support::ResetCurrentTestRoot("input-recording");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("input-recording");
        }
    };
}

TEST_F(InputRecordingTests, RecorderCapturesPolledFramesAcrossDefaultDeviceTypes)
{
    InputDeviceManager manager;
    InputRecorder recorder;

    auto *keyboard = new ScriptedKeyboardDevice();
    ScriptedKeyboardDevice::Frame keyboardFrame0{};
    keyboardFrame0[static_cast<std::size_t>(Key::W)] = 1u;
    keyboard->PushFrame(keyboardFrame0);

    ScriptedKeyboardDevice::Frame keyboardFrame1{};
    keyboardFrame1[static_cast<std::size_t>(Key::Space)] = 1u;
    keyboard->PushFrame(keyboardFrame1);

    auto *mouse = new ScriptedMouseDevice();
    ScriptedMouseDevice::Frame mouseFrame0;
    mouseFrame0.Buttons[Mouse::Left] = 1;
    mouseFrame0.X = 10.0f;
    mouseFrame0.Y = 20.0f;
    mouseFrame0.ScrollDelta = 1.5f;
    mouse->PushFrame(mouseFrame0);

    ScriptedMouseDevice::Frame mouseFrame1;
    mouseFrame1.Buttons[Mouse::Right] = 1;
    mouseFrame1.X = 15.0f;
    mouseFrame1.Y = 5.0f;
    mouseFrame1.ScrollDelta = -0.5f;
    mouse->PushFrame(mouseFrame1);

    auto *gamepad = new ScriptedGamepadDevice(1);
    ScriptedGamepadDevice::Frame gamepadFrame0;
    gamepadFrame0.Connected = true;
    gamepadFrame0.Buttons[GamepadButton::A] = 1;
    gamepadFrame0.Axes[GamepadAxis::LeftX] = 0.25f;
    gamepad->PushFrame(gamepadFrame0);

    ScriptedGamepadDevice::Frame gamepadFrame1;
    gamepadFrame1.Connected = true;
    gamepadFrame1.Buttons[GamepadButton::B] = 1;
    gamepadFrame1.Axes[GamepadAxis::RightY] = -0.75f;
    gamepad->PushFrame(gamepadFrame1);

    manager.AddDevice(Scope<InputDevice>(keyboard));
    manager.AddDevice(Scope<InputDevice>(mouse));
    manager.AddDevice(Scope<InputDevice>(gamepad));

    recorder.Attach(manager);
    recorder.BeginRecording();

    manager.PollAll(0.016f);
    manager.PollAll(0.033f);
    recorder.EndRecording();

    const auto &recording = recorder.GetRecording();
    ASSERT_EQ(recording.Frames.size(), 2u);

    const auto &frame0 = recording.Frames[0];
    EXPECT_EQ(frame0.FrameNumber, 0u);
    EXPECT_FLOAT_EQ(frame0.DeltaTime, 0.016f);
    EXPECT_TRUE(frame0.Keyboard.Present);
    EXPECT_EQ(frame0.Keyboard.Keys[Key::W], 1u);
    EXPECT_EQ(frame0.Keyboard.Keys[Key::Space], 0u);
    EXPECT_TRUE(frame0.Mouse.Present);
    EXPECT_EQ(frame0.Mouse.Buttons[Mouse::Left], 1u);
    EXPECT_FLOAT_EQ(frame0.Mouse.X, 10.0f);
    EXPECT_FLOAT_EQ(frame0.Mouse.Y, 20.0f);
    EXPECT_FLOAT_EQ(frame0.Mouse.ScrollDelta, 1.5f);
    ASSERT_EQ(frame0.Gamepads.size(), 1u);
    EXPECT_EQ(frame0.Gamepads[0].DeviceIndex, 1);
    EXPECT_TRUE(frame0.Gamepads[0].Connected);
    EXPECT_EQ(frame0.Gamepads[0].Buttons[GamepadButton::A], 1u);
    EXPECT_FLOAT_EQ(frame0.Gamepads[0].Axes[GamepadAxis::LeftX], 0.25f);

    const auto &frame1 = recording.Frames[1];
    EXPECT_EQ(frame1.FrameNumber, 1u);
    EXPECT_FLOAT_EQ(frame1.DeltaTime, 0.033f);
    EXPECT_EQ(frame1.Keyboard.Keys[Key::W], 0u);
    EXPECT_EQ(frame1.Keyboard.Keys[Key::Space], 1u);
    EXPECT_EQ(frame1.Mouse.Buttons[Mouse::Right], 1u);
    EXPECT_FLOAT_EQ(frame1.Mouse.X, 15.0f);
    EXPECT_FLOAT_EQ(frame1.Mouse.Y, 5.0f);
    EXPECT_FLOAT_EQ(frame1.Mouse.ScrollDelta, -0.5f);
    ASSERT_EQ(frame1.Gamepads.size(), 1u);
    EXPECT_EQ(frame1.Gamepads[0].Buttons[GamepadButton::B], 1u);
    EXPECT_FLOAT_EQ(frame1.Gamepads[0].Axes[GamepadAxis::RightY], -0.75f);
}

TEST_F(InputRecordingTests, ReplayDevicesExposeCurrentAndPreviousRecordedState)
{
    InputRecording recording;

    InputFrame frame0;
    frame0.FrameNumber = 0;
    frame0.DeltaTime = 0.016f;
    frame0.Keyboard.Present = true;
    frame0.Keyboard.Connected = true;
    frame0.Keyboard.Keys[Key::Space] = 1;
    frame0.Mouse.Present = true;
    frame0.Mouse.Connected = true;
    frame0.Mouse.Buttons[Mouse::Left] = 1;
    frame0.Mouse.X = 100.0f;
    frame0.Mouse.Y = 50.0f;
    frame0.Mouse.ScrollDelta = 1.0f;
    RecordedGamepadState gamepad0;
    gamepad0.DeviceIndex = 1;
    gamepad0.Connected = true;
    gamepad0.Buttons[GamepadButton::A] = 1;
    gamepad0.Axes[GamepadAxis::LeftX] = 0.5f;
    frame0.Gamepads.push_back(gamepad0);
    recording.Frames.push_back(frame0);

    InputFrame frame1;
    frame1.FrameNumber = 1;
    frame1.DeltaTime = 0.033f;
    frame1.Keyboard.Present = true;
    frame1.Keyboard.Connected = true;
    frame1.Mouse.Present = true;
    frame1.Mouse.Connected = true;
    frame1.Mouse.Buttons[Mouse::Right] = 1;
    frame1.Mouse.X = 110.0f;
    frame1.Mouse.Y = 40.0f;
    frame1.Mouse.ScrollDelta = 0.0f;
    RecordedGamepadState gamepad1;
    gamepad1.DeviceIndex = 1;
    gamepad1.Connected = false;
    frame1.Gamepads.push_back(gamepad1);
    recording.Frames.push_back(frame1);

    InputReplaySession session;
    session.SetRecording(std::move(recording));

    InputDeviceManager manager;
    session.Attach(manager);
    manager.AddDevice(CreateScope<ReplayKeyboardDevice>(session));
    manager.AddDevice(CreateScope<ReplayMouseDevice>(session));
    manager.AddDevice(CreateScope<ReplayGamepadDevice>(session, 1));

    manager.PollAll(0.016f);
    auto *keyboard = manager.GetDevice(InputDevice::Type::Keyboard);
    auto *mouse = manager.GetDevice(InputDevice::Type::Mouse);
    auto *gamepad = manager.GetDevice(InputDevice::Type::Gamepad, 1);
    ASSERT_NE(keyboard, nullptr);
    ASSERT_NE(mouse, nullptr);
    ASSERT_NE(gamepad, nullptr);

    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).X, 1.0f);
    EXPECT_FLOAT_EQ(keyboard->GetPreviousInput(Key::Space).X, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Left).X, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::PositionX).X, 100.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaX).X, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::ScrollY).X, 1.0f);
    EXPECT_TRUE(gamepad->IsConnected());
    EXPECT_FLOAT_EQ(gamepad->GetInput(GamepadButton::A).X, 1.0f);
    EXPECT_FLOAT_EQ(gamepad->GetAxis(GamepadAxis::LeftX).X, 0.5f);

    manager.PollAll(0.033f);
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).X, 0.0f);
    EXPECT_FLOAT_EQ(keyboard->GetPreviousInput(Key::Space).X, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Left).X, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetInput(Mouse::Right).X, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousInput(Mouse::Left).X, 1.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::PositionX).X, 110.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::PositionY).X, 40.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaX).X, 10.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaY).X, -10.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxisId::ScrollY).X, 1.0f);
    EXPECT_FALSE(gamepad->IsConnected());
    EXPECT_TRUE(gamepad->WasConnected());
    EXPECT_FLOAT_EQ(gamepad->GetInput(GamepadButton::A).X, 0.0f);
    EXPECT_FLOAT_EQ(gamepad->GetPreviousInput(GamepadButton::A).X, 1.0f);

    manager.PollAll(0.05f);
    EXPECT_TRUE(session.IsFinished());
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Space).X, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::PositionX).X, 110.0f);
    EXPECT_FLOAT_EQ(mouse->GetAxis(MouseAxisId::DeltaX).X, 0.0f);
    EXPECT_FLOAT_EQ(mouse->GetPreviousAxis(MouseAxisId::DeltaX).X, 10.0f);
    EXPECT_FALSE(gamepad->IsConnected());
}

TEST_F(InputRecordingTests, ReplaySessionFlagsDeltaTimeMismatchWithoutStoppingPlayback)
{
    InputRecording recording;
    InputFrame frame;
    frame.FrameNumber = 0;
    frame.DeltaTime = 0.01f;
    frame.Keyboard.Present = true;
    frame.Keyboard.Connected = true;
    frame.Keyboard.Keys[Key::Tab] = 1;
    recording.Frames.push_back(frame);

    InputReplaySession session;
    session.SetRecording(std::move(recording));

    InputDeviceManager manager;
    session.Attach(manager);
    manager.AddDevice(CreateScope<ReplayKeyboardDevice>(session));

    manager.PollAll(0.03f);

    auto *keyboard = manager.GetDevice(InputDevice::Type::Keyboard);
    ASSERT_NE(keyboard, nullptr);
    EXPECT_TRUE(session.HasTimingMismatch());
    EXPECT_FLOAT_EQ(keyboard->GetInput(Key::Tab).X, 1.0f);
}
