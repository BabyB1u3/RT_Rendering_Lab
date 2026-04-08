#include <gtest/gtest.h>
#include "core/Time.h"

TEST(TimeTests, ResetClearsState)
{
    Time::Reset();

    EXPECT_DOUBLE_EQ(Time::GetDeltaTime(), 0.0f);
    EXPECT_DOUBLE_EQ(Time::GetTotalTime(), 0.0f);
}

TEST(TimeTests, FirstUpdateSetsAbsoluteTimeAndZeroDelta)
{
    Time::Reset();
    Time::Update(10.0);

    EXPECT_DOUBLE_EQ(Time::GetDeltaTime(), 0.0f);
    EXPECT_DOUBLE_EQ(Time::GetTotalTime(), 0.0f);
}

TEST(TimeTests, SecondUpdateComputesDeltaAndUpdatesAbsoluteTime)
{
    Time::Reset();
    Time::Update(10.0);
    Time::Update(10.5);

    EXPECT_NEAR(Time::GetDeltaTime(), 0.5f, 1e-5f);
    EXPECT_NEAR(Time::GetTotalTime(), 0.5f, 1e-5f);
}

TEST(TimeTests, MultipleUpdatesAlwaysKeepLatestAbsoluteTime)
{
    Time::Reset();
    Time::Update(1.0);
    Time::Update(1.25);
    Time::Update(2.0);

    EXPECT_NEAR(Time::GetDeltaTime(), 0.75f, 1e-5f);
    EXPECT_NEAR(Time::GetTotalTime(), 1.0f, 1e-5f);
}

TEST(TimeTests, ResetAfterUpdatesClearsBackToZero)
{
    Time::Reset();
    Time::Update(3.0);
    Time::Update(3.5);

    Time::Reset();

    EXPECT_DOUBLE_EQ(Time::GetDeltaTime(), 0.0f);
    EXPECT_DOUBLE_EQ(Time::GetTotalTime(), 0.0f);
}