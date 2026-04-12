#include <gtest/gtest.h>

#include "Core/Input/Input.h"
#include "Core/Input/Action/InputAction.h"
#include "Core/Input/Action/InputActionSerialization.h"
#include "InputActionTestSupport.h"
#include "InputTestAccess.h"

namespace
{
    using test_support::ConstantTrigger;
    using test_support::FixedValueModifier;
    using test_support::InputTestAccess;

    class InputActionMapTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            InputTestAccess::RestoreDefaultDevices();
            Input::Initialize(nullptr);
        }

        static InputTestAccess::FrameState MakeFrame()
        {
            return InputTestAccess::MakeFrame();
        }

        static void ApplyFrame(const InputTestAccess::FrameState &frame)
        {
            InputTestAccess::ApplyFrame(frame);
        }

        void UpdateWithFrame(const InputTestAccess::FrameState &frame, float dt = 0.016f)
        {
            ApplyFrame(frame);
            map.Update(dt);
        }

        InputActionMap map;
    };
} // namespace

TEST_F(InputActionMapTest, BindActionAndHasActionReturnsTrue)
{
    map.BindAction("Jump", Key::Space);

    EXPECT_TRUE(map.HasAction("Jump"));
    ASSERT_TRUE(map.GetActions().contains("Jump"));
    ASSERT_EQ(map.GetActions().at("Jump").size(), 1u);
    EXPECT_EQ(map.GetActions().at("Jump")[0].SourceType, InputSource::Type::Key);
    EXPECT_EQ(map.GetActions().at("Jump")[0].Code, Key::Space);
}

TEST_F(InputActionMapTest, BindAxisAndHasAxisReturnsTrue)
{
    map.BindAxis("MoveX", Key::D, Key::A);

    EXPECT_TRUE(map.HasAxis("MoveX"));
    ASSERT_TRUE(map.GetAxes().contains("MoveX"));
    EXPECT_EQ(map.GetAxes().at("MoveX").kind, InputActionMap::AxisEntry::Kind::KeyPair);
}

TEST_F(InputActionMapTest, BindActionMultipleTimesAccumulatesSources)
{
    map.BindAction("Confirm", Key::Enter);
    map.BindAction("Confirm", InputSource::FromMouseButton(Mouse::Left));

    ASSERT_TRUE(map.GetActions().contains("Confirm"));
    ASSERT_EQ(map.GetActions().at("Confirm").size(), 2u);
    EXPECT_EQ(map.GetActions().at("Confirm")[1].SourceType, InputSource::Type::MouseButton);
    EXPECT_EQ(map.GetActions().at("Confirm")[1].Code, Mouse::Left);
}

TEST_F(InputActionMapTest, BindChordActionStoresChordBinding)
{
    map.BindChordAction("Save", {InputSource::FromKey(Key::LeftControl), InputSource::FromKey(Key::S)});

    ASSERT_TRUE(map.GetChordActions().contains("Save"));
    ASSERT_EQ(map.GetChordActions().at("Save").size(), 1u);
    ASSERT_EQ(map.GetChordActions().at("Save")[0].Sources.size(), 2u);
    EXPECT_EQ(map.GetChordActions().at("Save")[0].Sources[0].Code, Key::LeftControl);
    EXPECT_EQ(map.GetChordActions().at("Save")[0].Sources[1].Code, Key::S);
}

TEST_F(InputActionMapTest, BindAxisSameNameOverwritesPreviousBinding)
{
    map.BindAxis("Look", Key::D, Key::A);
    map.BindAxis("Look", InputActionMap::MouseAxis::Y);

    ASSERT_TRUE(map.GetAxes().contains("Look"));
    EXPECT_EQ(map.GetAxes().at("Look").kind, InputActionMap::AxisEntry::Kind::MouseAxis);
    EXPECT_EQ(map.GetAxes().at("Look").mouseAxis, InputActionMap::MouseAxis::Y);
}

TEST_F(InputActionMapTest, UnknownQueriesReturnDefaults)
{
    EXPECT_FALSE(map.HasAction("UnknownAction"));
    EXPECT_FALSE(map.HasAxis("UnknownAxis"));
    EXPECT_FALSE(map.IsActionDown("UnknownAction"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("UnknownAction"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("UnknownAction"));
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("UnknownAction"));
    EXPECT_EQ(map.GetActionTriggerState("UnknownAction"), TriggerState::None);
    EXPECT_FLOAT_EQ(map.GetAxis("UnknownAxis"), 0.0f);
}

TEST_F(InputActionMapTest, UnbindRemovesDefinitionsAndRuntimeState)
{
    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::D, true);

    map.BindAction("Jump", Key::Space);
    map.BindAxis("MoveX", Key::D, Key::A);
    map.AddModifier("MoveX", std::make_unique<FixedValueModifier>(0.5f));
    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::Triggered));
    UpdateWithFrame(frame);

    map.Unbind("Jump");
    map.Unbind("MoveX");

    EXPECT_FALSE(map.HasAction("Jump"));
    EXPECT_FALSE(map.HasAxis("MoveX"));
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::None);
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.0f);
}

TEST_F(InputActionMapTest, ClearRemovesAllBindingsAndRuntimeState)
{
    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::Space, true);

    map.BindAction("Jump", Key::Space);
    map.BindAxis("MoveX", Key::D, Key::A);
    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::Triggered));
    UpdateWithFrame(frame);

    map.Clear();

    EXPECT_FALSE(map.HasAction("Jump"));
    EXPECT_FALSE(map.HasAxis("MoveX"));
    EXPECT_FALSE(map.IsActionDown("Jump"));
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::None);
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.0f);
}

TEST_F(InputActionMapTest, KeyActionTracksDownPressAndReleaseAcrossFrames)
{
    map.BindAction("Jump", Key::Space);

    auto pressedFrame = MakeFrame();
    InputTestAccess::SetKey(pressedFrame, Key::Space, true);
    ApplyFrame(pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Jump"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Jump"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Jump"));

    ApplyFrame(pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Jump"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Jump"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Jump"));

    ApplyFrame(MakeFrame());
    EXPECT_FALSE(map.IsActionDown("Jump"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Jump"));
    EXPECT_TRUE(map.WasActionReleasedThisFrame("Jump"));
}

TEST_F(InputActionMapTest, MouseButtonActionTracksDownPressAndReleaseAcrossFrames)
{
    map.BindAction("Fire", InputSource::FromMouseButton(Mouse::Left));

    auto pressedFrame = MakeFrame();
    InputTestAccess::SetMouseButton(pressedFrame, Mouse::Left, true);
    ApplyFrame(pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Fire"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Fire"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Fire"));

    ApplyFrame(pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Fire"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Fire"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Fire"));

    ApplyFrame(MakeFrame());
    EXPECT_FALSE(map.IsActionDown("Fire"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Fire"));
    EXPECT_TRUE(map.WasActionReleasedThisFrame("Fire"));
}

TEST_F(InputActionMapTest, MultipleActionSourcesUseOrSemantics)
{
    map.BindAction("Confirm", Key::Enter);
    map.BindAction("Confirm", Key::Space);

    auto enterFrame = MakeFrame();
    InputTestAccess::SetKey(enterFrame, Key::Enter, true);
    ApplyFrame(enterFrame);
    EXPECT_TRUE(map.IsActionDown("Confirm"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Confirm"));

    ApplyFrame(enterFrame);

    auto spaceFrame = MakeFrame();
    InputTestAccess::SetKey(spaceFrame, Key::Enter, true);
    InputTestAccess::SetKey(spaceFrame, Key::Space, true);
    ApplyFrame(spaceFrame);
    EXPECT_TRUE(map.IsActionDown("Confirm"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Confirm"));
}

TEST_F(InputActionMapTest, CapturedInputBlocksActionQueries)
{
    map.BindAction("Jump", Key::Space);

    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::Space, true);
    ApplyFrame(frame);

    Input::SetKeyboardCaptured(true);
    EXPECT_FALSE(map.IsActionDown("Jump"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Jump"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Jump"));
}

TEST_F(InputActionMapTest, KeyPairAxisReturnsPositiveNegativeAndZero)
{
    map.BindAxis("MoveX", Key::D, Key::A);

    auto positiveFrame = MakeFrame();
    InputTestAccess::SetKey(positiveFrame, Key::D, true);
    ApplyFrame(positiveFrame);
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 1.0f);

    auto negativeFrame = MakeFrame();
    InputTestAccess::SetKey(negativeFrame, Key::A, true);
    ApplyFrame(negativeFrame);
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), -1.0f);

    auto bothFrame = MakeFrame();
    InputTestAccess::SetKey(bothFrame, Key::D, true);
    InputTestAccess::SetKey(bothFrame, Key::A, true);
    ApplyFrame(bothFrame);
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.0f);

    ApplyFrame(MakeFrame());
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.0f);
}

TEST_F(InputActionMapTest, MouseXAxisUsesDelta)
{
    map.BindAxis("LookX", InputActionMap::MouseAxis::X);

    auto firstFrame = MakeFrame();
    InputTestAccess::SetMousePosition(firstFrame, 10.0f, 20.0f);
    ApplyFrame(firstFrame);

    auto secondFrame = MakeFrame();
    InputTestAccess::SetMousePosition(secondFrame, 24.0f, 18.0f);
    ApplyFrame(secondFrame);

    EXPECT_FLOAT_EQ(map.GetAxis("LookX"), 14.0f);
}

TEST_F(InputActionMapTest, MouseYAxisUsesDelta)
{
    map.BindAxis("LookY", InputActionMap::MouseAxis::Y);

    auto firstFrame = MakeFrame();
    InputTestAccess::SetMousePosition(firstFrame, 50.0f, 75.0f);
    ApplyFrame(firstFrame);

    auto secondFrame = MakeFrame();
    InputTestAccess::SetMousePosition(secondFrame, 40.0f, 90.0f);
    ApplyFrame(secondFrame);

    EXPECT_FLOAT_EQ(map.GetAxis("LookY"), 15.0f);
}

TEST_F(InputActionMapTest, MouseScrollAxisUsesScrollDelta)
{
    map.BindAxis("Zoom", InputActionMap::MouseAxis::ScrollY);

    Input::AccumulateScroll(2.0f);
    Input::AccumulateScroll(-0.5f);
    ApplyFrame(MakeFrame());

    EXPECT_FLOAT_EQ(map.GetAxis("Zoom"), 1.5f);
}

TEST_F(InputActionMapTest, GetAxisReturnsRawValueBeforeUpdateWhenModifierExists)
{
    map.BindAxis("MoveX", Key::D, Key::A);
    map.AddModifier("MoveX", std::make_unique<Sensitivity>(3.0f));

    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::D, true);
    ApplyFrame(frame);

    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 1.0f);
}

TEST_F(InputActionMapTest, UpdateCachesModifiedAxisValue)
{
    map.BindAxis("MoveX", Key::D, Key::A);
    map.AddModifier("MoveX", std::make_unique<DeadZone>(0.25f));
    map.AddModifier("MoveX", std::make_unique<Sensitivity>(2.0f));

    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::D, true);
    UpdateWithFrame(frame);

    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 2.0f);
}

TEST_F(InputActionMapTest, ResetRuntimeStateClearsAxisCache)
{
    map.BindAxis("MoveX", Key::D, Key::A);
    map.AddModifier("MoveX", std::make_unique<FixedValueModifier>(0.5f));

    UpdateWithFrame(MakeFrame());
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.5f);

    map.ResetRuntimeState();
    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), 0.0f);
}

TEST_F(InputActionMapTest, DefaultTriggerFallsBackToPressedBehavior)
{
    map.BindAction("Jump", Key::Space);

    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::Space, true);
    ApplyFrame(frame);

    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::Triggered);
}

TEST_F(InputActionMapTest, ExplicitTriggerRequiresUpdate)
{
    map.BindAction("Jump", Key::Space);
    map.SetTrigger("Jump", std::make_unique<PressedTrigger>());

    auto frame = MakeFrame();
    InputTestAccess::SetKey(frame, Key::Space, true);
    ApplyFrame(frame);

    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::None);
}

TEST_F(InputActionMapTest, ConstantTriggerUpdatesTriggeredAndOngoingState)
{
    map.BindAction("Jump", Key::Space);
    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::Triggered));
    UpdateWithFrame(MakeFrame());

    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::Triggered);

    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::Ongoing));
    UpdateWithFrame(MakeFrame());

    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::Ongoing);
}

TEST_F(InputActionMapTest, SetTriggerNullRestoresDefaultPressedBehavior)
{
    map.BindAction("Jump", Key::Space);
    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::None));

    auto pressedFrame = MakeFrame();
    InputTestAccess::SetKey(pressedFrame, Key::Space, true);
    ApplyFrame(pressedFrame);
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));

    map.SetTrigger("Jump", nullptr);
    ApplyFrame(MakeFrame());
    ApplyFrame(pressedFrame);

    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::Triggered);
}

TEST_F(InputActionMapTest, ResetRuntimeStateClearsTriggerCacheAndCallsReset)
{
    int resetCount = 0;
    map.BindAction("Jump", Key::Space);
    map.SetTrigger("Jump", std::make_unique<ConstantTrigger>(TriggerState::Triggered, &resetCount));
    UpdateWithFrame(MakeFrame());

    ASSERT_TRUE(map.WasActionTriggeredThisFrame("Jump"));

    map.ResetRuntimeState();

    EXPECT_EQ(resetCount, 1);
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Jump"));
    EXPECT_EQ(map.GetActionTriggerState("Jump"), TriggerState::None);
}

TEST_F(InputActionMapTest, HoldTriggerAdvancesAcrossFrames)
{
    map.BindAction("Charge", Key::Space);
    map.SetTrigger("Charge", std::make_unique<HoldTrigger>(0.05f));

    auto heldFrame = MakeFrame();
    InputTestAccess::SetKey(heldFrame, Key::Space, true);

    UpdateWithFrame(heldFrame, 0.02f);
    EXPECT_EQ(map.GetActionTriggerState("Charge"), TriggerState::Ongoing);

    UpdateWithFrame(heldFrame, 0.02f);
    EXPECT_EQ(map.GetActionTriggerState("Charge"), TriggerState::Ongoing);

    UpdateWithFrame(heldFrame, 0.02f);
    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Charge"));
    EXPECT_EQ(map.GetActionTriggerState("Charge"), TriggerState::Triggered);
}

TEST_F(InputActionMapTest, TapTriggerTriggersOnQuickRelease)
{
    map.BindAction("Dash", Key::Space);
    map.SetTrigger("Dash", std::make_unique<TapTrigger>(0.1f));

    auto pressedFrame = MakeFrame();
    InputTestAccess::SetKey(pressedFrame, Key::Space, true);
    UpdateWithFrame(pressedFrame, 0.02f);
    EXPECT_EQ(map.GetActionTriggerState("Dash"), TriggerState::Ongoing);

    UpdateWithFrame(MakeFrame(), 0.03f);
    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Dash"));
    EXPECT_EQ(map.GetActionTriggerState("Dash"), TriggerState::Triggered);
}

TEST_F(InputActionMapTest, DoubleTapTriggerTriggersOnSecondPress)
{
    map.BindAction("Dash", Key::Space);
    map.SetTrigger("Dash", std::make_unique<DoubleTapTrigger>(0.1f));

    auto pressedFrame = MakeFrame();
    InputTestAccess::SetKey(pressedFrame, Key::Space, true);

    UpdateWithFrame(pressedFrame, 0.01f);
    EXPECT_EQ(map.GetActionTriggerState("Dash"), TriggerState::Ongoing);

    UpdateWithFrame(MakeFrame(), 0.03f);
    EXPECT_EQ(map.GetActionTriggerState("Dash"), TriggerState::Ongoing);

    ApplyFrame(MakeFrame());
    UpdateWithFrame(pressedFrame, 0.03f);
    EXPECT_TRUE(map.WasActionTriggeredThisFrame("Dash"));
    EXPECT_EQ(map.GetActionTriggerState("Dash"), TriggerState::Triggered);
}

TEST_F(InputActionMapTest, ChordActionRequiresAllSourcesAndTracksEdges)
{
    map.BindChordAction("Save", {InputSource::FromKey(Key::LeftControl), InputSource::FromKey(Key::S)});

    auto ctrlOnly = MakeFrame();
    InputTestAccess::SetKey(ctrlOnly, Key::LeftControl, true);
    ApplyFrame(ctrlOnly);
    EXPECT_FALSE(map.IsActionDown("Save"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Save"));

    auto chordFrame = ctrlOnly;
    InputTestAccess::SetKey(chordFrame, Key::S, true);
    ApplyFrame(chordFrame);
    EXPECT_TRUE(map.IsActionDown("Save"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Save"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Save"));

    ApplyFrame(chordFrame);
    EXPECT_TRUE(map.IsActionDown("Save"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Save"));

    ApplyFrame(ctrlOnly);
    EXPECT_FALSE(map.IsActionDown("Save"));
    EXPECT_TRUE(map.WasActionReleasedThisFrame("Save"));
}

TEST_F(InputActionMapTest, SerializeAndDeserializeChordBindingsRemainCompatibleWithSingleSources)
{
    map.BindAction("Confirm", Key::Enter);
    map.BindChordAction("Save", {InputSource::FromKey(Key::LeftControl), InputSource::FromKey(Key::S)});

    Serialization::PropertyTree tree;
    Serialization::Serialize(tree, map);

    ASSERT_TRUE(tree.Contains("actions"));
    ASSERT_TRUE(tree["actions"].Contains("Confirm"));
    ASSERT_TRUE(tree["actions"].Contains("Save"));
    ASSERT_EQ(tree["actions"]["Confirm"].AsArray().size(), 1u);
    ASSERT_EQ(tree["actions"]["Save"].AsArray().size(), 1u);
    EXPECT_EQ(tree["actions"]["Save"][0]["kind"].AsString(), "Chord");

    InputActionMap roundTrip;
    ASSERT_TRUE(Serialization::Deserialize(tree, roundTrip));
    ASSERT_TRUE(roundTrip.GetActions().contains("Confirm"));
    ASSERT_TRUE(roundTrip.GetChordActions().contains("Save"));
    ASSERT_EQ(roundTrip.GetChordActions().at("Save").size(), 1u);
    ASSERT_EQ(roundTrip.GetChordActions().at("Save")[0].Sources.size(), 2u);
}

TEST_F(InputActionMapTest, GamepadButtonActionTracksDownPressAndReleaseAcrossFrames)
{
    map.BindAction("Jump", InputSource::FromGamepadButton(GamepadButton::A));

    auto pressedFrame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::SetGamepadButton(pressedFrame, GamepadButton::A, true);
    InputTestAccess::ApplyGamepadFrame(0, pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Jump"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Jump"));
    EXPECT_FALSE(map.WasActionReleasedThisFrame("Jump"));

    InputTestAccess::ApplyGamepadFrame(0, pressedFrame);
    EXPECT_TRUE(map.IsActionDown("Jump"));
    EXPECT_FALSE(map.WasActionPressedThisFrame("Jump"));

    InputTestAccess::ApplyGamepadFrame(0, InputTestAccess::MakeGamepadFrame(true));
    EXPECT_FALSE(map.IsActionDown("Jump"));
    EXPECT_TRUE(map.WasActionReleasedThisFrame("Jump"));
}

TEST_F(InputActionMapTest, GamepadAxisBindingReturnsCurrentAxisValue)
{
    map.BindAxis("MoveX", GamepadAxis::LeftX);

    auto frame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::SetGamepadAxis(frame, GamepadAxis::LeftX, -0.8f);
    InputTestAccess::ApplyGamepadFrame(0, frame);

    EXPECT_FLOAT_EQ(map.GetAxis("MoveX"), -0.8f);
}

TEST_F(InputActionMapTest, GamepadAxisSourceUsesThresholdForActionEdges)
{
    map.BindAction("Dash", InputSource::FromGamepadAxis(GamepadAxis::RightTrigger));

    auto activeFrame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::SetGamepadAxis(activeFrame, GamepadAxis::RightTrigger, 0.9f);
    InputTestAccess::ApplyGamepadFrame(0, activeFrame);
    EXPECT_TRUE(map.IsActionDown("Dash"));
    EXPECT_TRUE(map.WasActionPressedThisFrame("Dash"));

    auto inactiveFrame = InputTestAccess::MakeGamepadFrame(true);
    InputTestAccess::ApplyGamepadFrame(0, inactiveFrame);
    EXPECT_FALSE(map.IsActionDown("Dash"));
    EXPECT_TRUE(map.WasActionReleasedThisFrame("Dash"));
}
