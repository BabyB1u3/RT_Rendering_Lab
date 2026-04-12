#include <gtest/gtest.h>

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Code/KeyCode.h"
#include "Core/Input/Code/MouseCode.h"
#include "Core/Input/Replay/InputRecording.h"
#include "TestPaths.h"

namespace
{
    class InputRecordingIOTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            test_support::ResetCurrentTestRoot("input-recording-io");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("input-recording-io");
        }
    };
} // namespace

TEST_F(InputRecordingIOTests, RecorderBinaryRoundTripPreservesFrames)
{
    InputRecorder recorder;
    InputRecording recording;

    InputFrame frame;
    frame.FrameNumber = 7;
    frame.DeltaTime = 0.02f;
    frame.Keyboard.Present = true;
    frame.Keyboard.Connected = true;
    frame.Keyboard.Keys[Key::Enter] = 1;
    frame.Mouse.Present = true;
    frame.Mouse.Connected = true;
    frame.Mouse.Buttons[Mouse::Middle] = 1;
    frame.Mouse.X = 128.0f;
    frame.Mouse.Y = 256.0f;
    frame.Mouse.ScrollDelta = 3.0f;

    RecordedGamepadState gamepad;
    gamepad.DeviceIndex = 2;
    gamepad.Connected = true;
    gamepad.Buttons[GamepadButton::X] = 1;
    gamepad.Axes[GamepadAxis::LeftTrigger] = 0.9f;
    frame.Gamepads.push_back(gamepad);

    recording.Frames.push_back(frame);
    recorder.SetRecording(recording);

    const auto path = test_support::CurrentTestPath("input-recording-io", "capture/input_recording.bin");
    test_support::EnsureDirectories(path.parent_path());
    ASSERT_TRUE(recorder.SaveToFile(path));

    InputRecorder loaded;
    ASSERT_TRUE(loaded.LoadFromFile(path));

    const auto &loadedRecording = loaded.GetRecording();
    ASSERT_EQ(loadedRecording.Frames.size(), 1u);
    const auto &loadedFrame = loadedRecording.Frames[0];
    EXPECT_EQ(loadedFrame.FrameNumber, 7u);
    EXPECT_FLOAT_EQ(loadedFrame.DeltaTime, 0.02f);
    EXPECT_TRUE(loadedFrame.Keyboard.Present);
    EXPECT_EQ(loadedFrame.Keyboard.Keys[Key::Enter], 1u);
    EXPECT_TRUE(loadedFrame.Mouse.Present);
    EXPECT_EQ(loadedFrame.Mouse.Buttons[Mouse::Middle], 1u);
    EXPECT_FLOAT_EQ(loadedFrame.Mouse.X, 128.0f);
    EXPECT_FLOAT_EQ(loadedFrame.Mouse.Y, 256.0f);
    EXPECT_FLOAT_EQ(loadedFrame.Mouse.ScrollDelta, 3.0f);
    ASSERT_EQ(loadedFrame.Gamepads.size(), 1u);
    EXPECT_EQ(loadedFrame.Gamepads[0].DeviceIndex, 2);
    EXPECT_EQ(loadedFrame.Gamepads[0].Buttons[GamepadButton::X], 1u);
    EXPECT_FLOAT_EQ(loadedFrame.Gamepads[0].Axes[GamepadAxis::LeftTrigger], 0.9f);
}
