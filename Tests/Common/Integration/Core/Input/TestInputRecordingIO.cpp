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
    void SetUp() override { test_support::ResetCurrentTestRoot("input-recording-io"); }

    void TearDown() override { test_support::RemoveCurrentTestArtifacts("input-recording-io"); }
};
} // namespace

TEST_F(InputRecordingIOTests, RecorderBinaryRoundTripPreservesFrames)
{
    InputRecorder recorder;
    InputRecording recording;

    InputFrame frame;
    frame.frameNumber = 7;
    frame.deltaTime = 0.02f;
    frame.keyboard.isPresent = true;
    frame.keyboard.isConnected = true;
    frame.keyboard.keys[Key::Enter] = 1;
    frame.mouse.isPresent = true;
    frame.mouse.isConnected = true;
    frame.mouse.buttons[Mouse::Middle] = 1;
    frame.mouse.x = 128.0f;
    frame.mouse.y = 256.0f;
    frame.mouse.scrollDelta = 3.0f;

    RecordedGamepadState gamepad;
    gamepad.deviceIndex = 2;
    gamepad.isConnected = true;
    gamepad.buttons[GamepadButton::X] = 1;
    gamepad.axes[GamepadAxis::LeftTrigger] = 0.9f;
    frame.gamepads.push_back(gamepad);

    recording.frames.push_back(frame);
    recorder.SetRecording(recording);

    const auto path = test_support::CurrentTestPath("input-recording-io", "capture/input_recording.bin");
    test_support::EnsureDirectories(path.parent_path());
    ASSERT_TRUE(recorder.SaveToFile(path));

    InputRecorder loaded;
    ASSERT_TRUE(loaded.LoadFromFile(path));

    const auto& loadedRecording = loaded.GetRecording();
    ASSERT_EQ(loadedRecording.frames.size(), 1u);
    const auto& loadedFrame = loadedRecording.frames[0];
    EXPECT_EQ(loadedFrame.frameNumber, 7u);
    EXPECT_FLOAT_EQ(loadedFrame.deltaTime, 0.02f);
    EXPECT_TRUE(loadedFrame.keyboard.isPresent);
    EXPECT_EQ(loadedFrame.keyboard.keys[Key::Enter], 1u);
    EXPECT_TRUE(loadedFrame.mouse.isPresent);
    EXPECT_EQ(loadedFrame.mouse.buttons[Mouse::Middle], 1u);
    EXPECT_FLOAT_EQ(loadedFrame.mouse.x, 128.0f);
    EXPECT_FLOAT_EQ(loadedFrame.mouse.y, 256.0f);
    EXPECT_FLOAT_EQ(loadedFrame.mouse.scrollDelta, 3.0f);
    ASSERT_EQ(loadedFrame.gamepads.size(), 1u);
    EXPECT_EQ(loadedFrame.gamepads[0].deviceIndex, 2);
    EXPECT_EQ(loadedFrame.gamepads[0].buttons[GamepadButton::X], 1u);
    EXPECT_FLOAT_EQ(loadedFrame.gamepads[0].axes[GamepadAxis::LeftTrigger], 0.9f);
}
