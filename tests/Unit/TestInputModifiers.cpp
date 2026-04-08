#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "core/input/action/InputModifier.h"

// --- DeadZone ---

TEST(InputModifierTests, DeadZoneZeroesOutValuesBelowThreshold)
{
    DeadZone dz(0.1f);
    EXPECT_FLOAT_EQ(dz.Apply(0.05f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(dz.Apply(-0.05f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(dz.Apply(0.0f, 0.0f), 0.0f);
}

TEST(InputModifierTests, DeadZonePassesThroughValuesAboveThreshold)
{
    DeadZone dz(0.1f);
    EXPECT_FLOAT_EQ(dz.Apply(0.5f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(dz.Apply(-0.5f, 0.0f), -0.5f);
    EXPECT_FLOAT_EQ(dz.Apply(0.1f, 0.0f), 0.1f);
}

TEST(InputModifierTests, DeadZonePassesThroughNegativeValueAtThreshold)
{
    DeadZone dz(0.1f);
    EXPECT_FLOAT_EQ(dz.Apply(-0.1f, 0.0f), -0.1f);
}

// --- Sensitivity ---

TEST(InputModifierTests, SensitivityScalesValue)
{
    Sensitivity sens(2.0f);
    EXPECT_FLOAT_EQ(sens.Apply(0.5f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(sens.Apply(-0.3f, 0.0f), -0.6f);
    EXPECT_FLOAT_EQ(sens.Apply(0.0f, 0.0f), 0.0f);
}

TEST(InputModifierTests, SensitivityNegativeScaleFlipsSign)
{
    Sensitivity sens(-2.0f);
    EXPECT_FLOAT_EQ(sens.Apply(0.5f, 0.0f), -1.0f);
    EXPECT_FLOAT_EQ(sens.Apply(-0.25f, 0.0f), 0.5f);
}

// --- Clamp ---

TEST(InputModifierTests, ClampRestrictsValueToRange)
{
    Clamp clamp(-0.5f, 0.5f);
    EXPECT_FLOAT_EQ(clamp.Apply(1.0f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(clamp.Apply(-1.0f, 0.0f), -0.5f);
    EXPECT_FLOAT_EQ(clamp.Apply(0.3f, 0.0f), 0.3f);
}

TEST(InputModifierTests, ClampKeepsExactBoundaryValues)
{
    Clamp clamp(-0.5f, 0.5f);
    EXPECT_FLOAT_EQ(clamp.Apply(-0.5f, 0.0f), -0.5f);
    EXPECT_FLOAT_EQ(clamp.Apply(0.5f, 0.0f), 0.5f);
}

// --- Negate ---

TEST(InputModifierTests, NegateInvertsValue)
{
    Negate neg;
    EXPECT_FLOAT_EQ(neg.Apply(1.0f, 0.0f), -1.0f);
    EXPECT_FLOAT_EQ(neg.Apply(-0.5f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(neg.Apply(0.0f, 0.0f), 0.0f);
}

// --- Smooth ---

TEST(InputModifierTests, SmoothConvergesTowardsTarget)
{
    Smooth smooth(0.1f);

    // Start from 0, target 1.0 should move toward 1.0 but not reach it instantly
    float result = smooth.Apply(1.0f, 0.016f);
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 1.0f);

    // Apply many times should converge close to 1.0
    for (int i = 0; i < 100; ++i)
        result = smooth.Apply(1.0f, 0.016f);
    EXPECT_NEAR(result, 1.0f, 0.01f);
}

TEST(InputModifierTests, SmoothFirstStepMatchesExpectedAlpha)
{
    constexpr float halfLife = 0.1f;
    constexpr float dt = 0.016f;
    constexpr float target = 1.0f;
    const float alpha = 1.0f - std::pow(2.0f, -dt / halfLife);

    Smooth smooth(halfLife);
    EXPECT_NEAR(smooth.Apply(target, dt), alpha * target, 1e-6f);
}

TEST(InputModifierTests, SmoothConvergesTowardsNegativeTarget)
{
    Smooth smooth(0.1f);

    float result = smooth.Apply(-1.0f, 0.016f);
    EXPECT_LT(result, 0.0f);
    EXPECT_GT(result, -1.0f);

    for (int i = 0; i < 100; ++i)
        result = smooth.Apply(-1.0f, 0.016f);
    EXPECT_NEAR(result, -1.0f, 0.01f);
}

TEST(InputModifierTests, SmoothLargerDeltaTimeMovesFurtherTowardsTarget)
{
    Smooth smallDt(0.1f);
    Smooth largeDt(0.1f);

    const float smallStep = smallDt.Apply(1.0f, 0.016f);
    const float largeStep = largeDt.Apply(1.0f, 0.064f);

    EXPECT_GT(largeStep, smallStep);
    EXPECT_LT(largeStep, 1.0f);
}

TEST(InputModifierTests, SmoothRetainsStateAcrossTargetChanges)
{
    constexpr float halfLife = 0.1f;
    constexpr float dt = 0.016f;
    const float alpha = 1.0f - std::pow(2.0f, -dt / halfLife);

    Smooth smooth(halfLife);
    const float first = smooth.Apply(1.0f, dt);
    const float second = smooth.Apply(-1.0f, dt);
    const float expectedSecond = first + alpha * (-1.0f - first);

    EXPECT_NEAR(second, expectedSecond, 1e-6f);
}

TEST(InputModifierTests, SmoothWithZeroDtPassesThroughUnchanged)
{
    Smooth smooth(0.1f);
    EXPECT_FLOAT_EQ(smooth.Apply(5.0f, 0.0f), 5.0f);
}

// --- Modifier chaining ---

TEST(InputModifierTests, ModifierChainAppliesInOrder)
{
    // DeadZone(0.05) Sensitivity(3.0) Clamp(-1, 1)
    std::vector<std::unique_ptr<InputModifier>> chain;
    chain.push_back(std::make_unique<DeadZone>(0.05f));
    chain.push_back(std::make_unique<Sensitivity>(3.0f));
    chain.push_back(std::make_unique<Clamp>(-1.0f, 1.0f));

    float value = 0.02f; // below dead zone
    for (const auto &mod : chain)
        value = mod->Apply(value, 0.0f);
    EXPECT_FLOAT_EQ(value, 0.0f); // zeroed by dead zone

    value = 0.4f; // above dead zone: 0.4 * 3.0 = 1.2, clamped to 1.0
    for (const auto &mod : chain)
        value = mod->Apply(value, 0.0f);
    EXPECT_FLOAT_EQ(value, 1.0f);

    value = 0.1f; // above dead zone: 0.1 * 3.0 = 0.3, within clamp
    for (const auto &mod : chain)
        value = mod->Apply(value, 0.0f);
    EXPECT_FLOAT_EQ(value, 0.3f);
}
