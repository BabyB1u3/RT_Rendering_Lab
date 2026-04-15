#include <gtest/gtest.h>

#include "Core/Input/Input.h"
#include "Core/Input/Action/InputPatterns.h"
#include "InputTestAccess.h"

namespace
{
using test_support::InputTestAccess;

class ComboTrackerTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InputTestAccess::RestoreDefaultDevices();
        Input::Initialize(nullptr);
    }
};
} // namespace

TEST_F(ComboTrackerTests, ComboTrackerCompletesSequenceWithinStepWindows)
{
    ComboTracker tracker;
    tracker.SetCombo(ComboBinding{{
        {InputSource::FromKey(Key::W), 0.0f},
        {InputSource::FromKey(Key::S), 0.1f},
        {InputSource::FromKey(Key::D), 0.1f},
    }});

    auto wFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(wFrame, Key::W, true);
    InputTestAccess::ApplyFrame(wFrame);
    EXPECT_FALSE(tracker.Update(0.016f));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(tracker.Update(0.03f));

    auto sFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(sFrame, Key::S, true);
    InputTestAccess::ApplyFrame(sFrame);
    EXPECT_FALSE(tracker.Update(0.03f));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(tracker.Update(0.03f));

    auto dFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(dFrame, Key::D, true);
    InputTestAccess::ApplyFrame(dFrame);
    EXPECT_TRUE(tracker.Update(0.03f));
    EXPECT_TRUE(tracker.IsIdle());
}

TEST_F(ComboTrackerTests, ComboTrackerResetsAfterTimeout)
{
    ComboTracker tracker;
    tracker.SetCombo(ComboBinding{{
        {InputSource::FromKey(Key::W), 0.0f},
        {InputSource::FromKey(Key::S), 0.05f},
    }});

    auto wFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(wFrame, Key::W, true);
    InputTestAccess::ApplyFrame(wFrame);
    EXPECT_FALSE(tracker.Update(0.016f));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(tracker.Update(0.06f));
    EXPECT_TRUE(tracker.IsIdle());
}

TEST_F(ComboTrackerTests, ComboTrackerCanRestartAfterTimeoutOnNewPress)
{
    ComboTracker tracker;
    tracker.SetCombo(ComboBinding{{
        {InputSource::FromKey(Key::W), 0.0f},
        {InputSource::FromKey(Key::S), 0.05f},
    }});

    auto wFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(wFrame, Key::W, true);
    InputTestAccess::ApplyFrame(wFrame);
    EXPECT_FALSE(tracker.Update(0.016f));

    InputTestAccess::ApplyFrame(InputTestAccess::MakeFrame());
    EXPECT_FALSE(tracker.Update(0.06f));

    auto restartFrame = InputTestAccess::MakeFrame();
    InputTestAccess::SetKey(restartFrame, Key::W, true);
    InputTestAccess::ApplyFrame(restartFrame);
    EXPECT_FALSE(tracker.Update(0.016f));
    EXPECT_EQ(tracker.GetCurrentStep(), 1u);
}
