#include <gtest/gtest.h>

#include "core/input/InputContextStack.h"
#include "core/input/InputAction.h"

namespace
{
    class FixedValueModifier final : public InputModifier
    {
    public:
        explicit FixedValueModifier(float value) : m_Value(value) {}

        float Apply(float /*value*/, float /*deltaTime*/) const override
        {
            return m_Value;
        }

    private:
        float m_Value;
    };

    class ConstantTrigger final : public InputTrigger
    {
    public:
        explicit ConstantTrigger(TriggerState state, int *resetCount = nullptr)
            : m_State(state), m_ResetCount(resetCount) {}

        TriggerState Evaluate(bool /*down*/, bool /*pressed*/, bool /*released*/, float /*dt*/) override
        {
            return m_State;
        }

        void Reset() override
        {
            if (m_ResetCount != nullptr)
                ++(*m_ResetCount);
        }

    private:
        TriggerState m_State;
        int *m_ResetCount;
    };

    void BindConstantAxis(InputActionMap &map, const std::string &name, float value)
    {
        map.BindAxis(name, Key::D, Key::A);
        map.AddModifier(name, std::make_unique<FixedValueModifier>(value));
    }

    void BindConstantTrigger(InputActionMap &map, const std::string &name, TriggerState state,
                             int *resetCount = nullptr)
    {
        map.BindAction(name, Key::Space);
        map.SetTrigger(name, std::make_unique<ConstantTrigger>(state, resetCount));
    }
}

// These tests verify the contract behavior of InputContextStack:
// priority ordering, highest-priority ownership, consumption blocking,
// active/inactive logic, and runtime-state reset on deactivate.

// --- Push / Pop / HasContext ---

TEST(InputContextStackTests, PushAddsContext)
{
    InputContextStack stack;
    InputActionMap map;

    stack.Push("Gameplay", &map);
    EXPECT_EQ(stack.Size(), 1u);
    EXPECT_TRUE(stack.HasContext("Gameplay"));
}

TEST(InputContextStackTests, PushSortsByPriorityDescending)
{
    InputContextStack stack;
    InputActionMap mapLow, mapMid, mapHigh;

    BindConstantAxis(mapLow, "MoveX", 1.0f);
    BindConstantAxis(mapMid, "MoveX", 2.0f);
    BindConstantAxis(mapHigh, "MoveX", 3.0f);

    stack.Push("Low", &mapLow, 0);
    stack.Push("High", &mapHigh, 100);
    stack.Push("Mid", &mapMid, 50);
    stack.Update(0.016f);

    EXPECT_EQ(stack.Size(), 3u);
    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 3.0f);
}

TEST(InputContextStackTests, PopRemovesContextByName)
{
    InputContextStack stack;
    InputActionMap mapA, mapB;

    stack.Push("A", &mapA);
    stack.Push("B", &mapB);
    EXPECT_EQ(stack.Size(), 2u);

    stack.Pop("A");
    EXPECT_EQ(stack.Size(), 1u);
    EXPECT_FALSE(stack.HasContext("A"));
    EXPECT_TRUE(stack.HasContext("B"));
}

TEST(InputContextStackTests, PopNonexistentNameIsNoOp)
{
    InputContextStack stack;
    InputActionMap map;

    stack.Push("A", &map);
    stack.Pop("DoesNotExist");
    EXPECT_EQ(stack.Size(), 1u);
}

TEST(InputContextStackTests, DuplicateNameReplacesExisting)
{
    InputContextStack stack;
    InputActionMap map1, map2;

    BindConstantAxis(map1, "MoveX", 1.0f);
    BindConstantAxis(map2, "MoveX", 5.0f);

    stack.Push("Gameplay", &map1, 0);
    stack.Push("Gameplay", &map2, 50);
    stack.Update(0.016f);

    EXPECT_EQ(stack.Size(), 1u);
    EXPECT_TRUE(stack.HasContext("Gameplay"));
    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 5.0f);
}

// --- SetActive ---

TEST(InputContextStackTests, SetActiveDisablesContext)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantAxis(highMap, "MoveX", 4.0f);
    BindConstantAxis(lowMap, "MoveX", 1.0f);

    stack.Push("Gameplay", &lowMap, 0);
    stack.Push("Overlay", &highMap, 100);
    stack.SetActive("Overlay", false);
    stack.Update(0.016f);

    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 1.0f);
}

TEST(InputContextStackTests, SetActiveReenablesContext)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantAxis(highMap, "MoveX", 4.0f);
    BindConstantAxis(lowMap, "MoveX", 1.0f);

    stack.Push("Gameplay", &lowMap, 0);
    stack.Push("Overlay", &highMap, 100);
    stack.SetActive("Overlay", false);
    stack.SetActive("Overlay", true);
    stack.Update(0.016f);

    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 4.0f);
}

TEST(InputContextStackTests, DeactivatingContextResetsRuntimeState)
{
    InputContextStack stack;
    InputActionMap map;
    int resetCount = 0;

    BindConstantTrigger(map, "Confirm", TriggerState::Triggered, &resetCount);
    stack.Push("Menu", &map, 100);
    stack.Update(0.016f);

    ASSERT_TRUE(map.WasActionTriggeredThisFrame("Confirm"));

    stack.SetActive("Menu", false);

    EXPECT_EQ(resetCount, 1);
    EXPECT_FALSE(map.WasActionTriggeredThisFrame("Confirm"));
    EXPECT_EQ(map.GetActionTriggerState("Confirm"), TriggerState::None);
}

// --- Consumption blocking ---

TEST(InputContextStackTests, ConsumingContextBlocksLowerPriorityUpdate)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantTrigger(lowMap, "GameplayOnly", TriggerState::Triggered);
    BindConstantTrigger(highMap, "Pause", TriggerState::None);

    stack.Push("Menu", &highMap, 100, /*consumesInput=*/true);
    stack.Push("Gameplay", &lowMap, 0);
    stack.Update(0.016f);

    EXPECT_FALSE(lowMap.WasActionTriggeredThisFrame("GameplayOnly"));
    EXPECT_FALSE(stack.WasActionTriggeredThisFrame("GameplayOnly"));
}

TEST(InputContextStackTests, InactiveConsumingContextDoesNotBlock)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantTrigger(lowMap, "GameplayOnly", TriggerState::Triggered);
    BindConstantTrigger(highMap, "Pause", TriggerState::None);

    stack.Push("Menu", &highMap, 100, /*consumesInput=*/true);
    stack.Push("Gameplay", &lowMap, 0);
    stack.SetActive("Menu", false);
    stack.Update(0.016f);

    EXPECT_TRUE(lowMap.WasActionTriggeredThisFrame("GameplayOnly"));
    EXPECT_TRUE(stack.WasActionTriggeredThisFrame("GameplayOnly"));
}

// --- Cross-context queries ---

TEST(InputContextStackTests, WasActionTriggeredUsesHighestPriorityContextThatDefinesAction)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantTrigger(highMap, "Confirm", TriggerState::None);
    BindConstantTrigger(lowMap, "Confirm", TriggerState::Triggered);

    stack.Push("Gameplay", &lowMap, 0);
    stack.Push("Menu", &highMap, 100);
    stack.Update(0.016f);

    EXPECT_FALSE(stack.WasActionTriggeredThisFrame("Confirm"));
}

TEST(InputContextStackTests, WasActionTriggeredFallsBackWhenHigherPriorityDoesNotDefineAction)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantTrigger(highMap, "Pause", TriggerState::None);
    BindConstantTrigger(lowMap, "Confirm", TriggerState::Triggered);

    stack.Push("Gameplay", &lowMap, 0);
    stack.Push("Menu", &highMap, 100);
    stack.Update(0.016f);

    EXPECT_TRUE(stack.WasActionTriggeredThisFrame("Confirm"));
}

TEST(InputContextStackTests, GetAxisUsesHighestPriorityContextThatDefinesAxisEvenWhenValueIsZero)
{
    InputContextStack stack;
    InputActionMap highMap, lowMap;

    BindConstantAxis(highMap, "MoveX", 0.0f);
    BindConstantAxis(lowMap, "MoveX", 5.0f);

    stack.Push("Gameplay", &lowMap, 0);
    stack.Push("Menu", &highMap, 100);
    stack.Update(0.016f);

    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 0.0f);
}

TEST(InputContextStackTests, EmptyStackQueriesReturnDefaults)
{
    InputContextStack stack;

    EXPECT_FALSE(stack.IsActionDown("Jump"));
    EXPECT_FALSE(stack.WasActionTriggeredThisFrame("Jump"));
    EXPECT_FLOAT_EQ(stack.GetAxis("MoveX"), 0.0f);
}

TEST(InputContextStackTests, UpdateOnEmptyStackIsNoOp)
{
    InputContextStack stack;

    stack.Update(0.016f);
    EXPECT_EQ(stack.Size(), 0u);
}
