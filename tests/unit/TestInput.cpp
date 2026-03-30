#include <gtest/gtest.h>

#include "core/input/Input.h"
#include "InputTestAccess.h"

namespace
{
    using test_support::InputTestAccess;

    class InputPollingTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
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
