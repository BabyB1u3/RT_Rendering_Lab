#include <gtest/gtest.h>

#include "core/input/action/InputTrigger.h"

// --- PressedTrigger ---

TEST(InputTriggerTests, PressedTriggerFiresOnPress)
{
    PressedTrigger trigger;

    EXPECT_EQ(trigger.Evaluate(false, false, false, 0.016f), TriggerState::None);
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Triggered);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.016f), TriggerState::None);
}

TEST(InputTriggerTests, PressedTriggerResetIsNoOp)
{
    PressedTrigger trigger;

    trigger.Reset();
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Triggered);
}

// --- ReleasedTrigger ---

TEST(InputTriggerTests, ReleasedTriggerFiresOnRelease)
{
    ReleasedTrigger trigger;

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::None);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.016f), TriggerState::None);
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::Triggered);
}

TEST(InputTriggerTests, ReleasedTriggerResetIsNoOp)
{
    ReleasedTrigger trigger;

    trigger.Reset();
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::Triggered);
}

// --- HoldTrigger ---

TEST(InputTriggerTests, HoldTriggerFiresAfterDuration)
{
    HoldTrigger trigger(0.5f);

    // Hold for less than duration Ongoing
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.2f), TriggerState::Ongoing);

    // Accumulate past threshold: 0.016 + 0.2 + 0.3 = 0.516 >= 0.5
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.3f), TriggerState::Triggered);

    // Already fired should not fire again while held
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.1f), TriggerState::None);
}

TEST(InputTriggerTests, HoldTriggerFiresExactlyAtDurationBoundary)
{
    HoldTrigger trigger(0.5f);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.2f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.3f), TriggerState::Triggered);
}

TEST(InputTriggerTests, HoldTriggerZeroDurationTriggersImmediatelyWhenHeld)
{
    HoldTrigger trigger(0.0f);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.0f), TriggerState::Triggered);
}

TEST(InputTriggerTests, HoldTriggerResetsOnRelease)
{
    HoldTrigger trigger(0.5f);

    // Hold partway
    trigger.Evaluate(true, true, false, 0.3f);

    // Release resets
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::None);

    // Hold again should need full duration again
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.3f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.3f), TriggerState::Triggered);
}

TEST(InputTriggerTests, HoldTriggerCanTriggerAgainAfterReleaseAndRepress)
{
    HoldTrigger trigger(0.2f);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.1f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.1f), TriggerState::Triggered);
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::None);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.1f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.1f), TriggerState::Triggered);
}

// --- TapTrigger ---

TEST(InputTriggerTests, TapTriggerFiresOnQuickRelease)
{
    TapTrigger trigger(0.2f);

    // Press
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);

    // Release quickly (0.016 + 0.05 = 0.066 < 0.2)
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.05f), TriggerState::Triggered);
}

TEST(InputTriggerTests, TapTriggerFiresWhenReleasedExactlyAtMaxDuration)
{
    TapTrigger trigger(0.2f);

    // dt on the press frame is not accumulated (TapTrigger returns early on pressed).
    // Total elapsed = 0.1 + 0.1 = 0.2 == maxDuration.
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.1f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.1f), TriggerState::Triggered);
}

TEST(InputTriggerTests, TapTriggerFailsOnSlowRelease)
{
    TapTrigger trigger(0.2f);

    // Press
    trigger.Evaluate(true, true, false, 0.016f);

    // Hold too long
    trigger.Evaluate(true, false, false, 0.1f);
    trigger.Evaluate(true, false, false, 0.1f);

    // Release after > 0.2s should NOT trigger
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::None);
}

TEST(InputTriggerTests, TapTriggerTimesOutWhileHeld)
{
    TapTrigger trigger(0.2f);

    // Press
    trigger.Evaluate(true, true, false, 0.016f);

    // Exceed max duration while still held window expires
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.25f), TriggerState::None);
}

TEST(InputTriggerTests, TapTriggerRemainsOngoingWhileHeldWithinWindow)
{
    TapTrigger trigger(0.2f);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.05f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.05f), TriggerState::Ongoing);
}

TEST(InputTriggerTests, TapTriggerCanTriggerAgainOnSecondQuickTap)
{
    TapTrigger trigger(0.2f);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.05f), TriggerState::Triggered);

    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.016f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.05f), TriggerState::Triggered);
}

// --- Reset ---

TEST(InputTriggerTests, HoldTriggerResetClearsState)
{
    HoldTrigger trigger(0.5f);

    trigger.Evaluate(true, true, false, 0.3f);
    trigger.Reset();

    // After reset, should need full duration again
    EXPECT_EQ(trigger.Evaluate(true, true, false, 0.3f), TriggerState::Ongoing);
    EXPECT_EQ(trigger.Evaluate(true, false, false, 0.3f), TriggerState::Triggered);
}

TEST(InputTriggerTests, TapTriggerResetClearsState)
{
    TapTrigger trigger(0.2f);

    trigger.Evaluate(true, true, false, 0.016f);
    trigger.Reset();

    // After reset, release should not trigger (no pending press)
    EXPECT_EQ(trigger.Evaluate(false, false, true, 0.016f), TriggerState::None);
}
