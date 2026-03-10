#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "core/LayerStack.h"
#include "core/Base.h"
#include "TestLayer.h"

TEST(LayerStackTests, PushLayerKeepsNormalLayerOrder)
{
    TestLayer::ResetCounters();

    LayerStack stack;

    auto *a = stack.PushLayer(CreateScope<TestLayer>("A"));
    auto *b = stack.PushLayer(CreateScope<TestLayer>("B"));

    std::vector<std::string> names;
    for (const auto &layer : stack)
        names.push_back(layer->GetName());

    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "A");
    EXPECT_EQ(names[1], "B");

    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_EQ(TestLayer::LiveCount(), 2);
}

TEST(LayerStackTests, OverlayIsAppendedAfterNormalLayers)
{
    TestLayer::ResetCounters();

    LayerStack stack;

    stack.PushLayer(CreateScope<TestLayer>("LayerA"));
    stack.PushLayer(CreateScope<TestLayer>("LayerB"));
    stack.PushOverlay(CreateScope<TestLayer>("OverlayA"));

    std::vector<std::string> names;
    for (const auto &layer : stack)
        names.push_back(layer->GetName());

    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "LayerA");
    EXPECT_EQ(names[1], "LayerB");
    EXPECT_EQ(names[2], "OverlayA");
}

TEST(LayerStackTests, PopLayerRemovesOnlyTargetLayer)
{
    TestLayer::ResetCounters();

    LayerStack stack;

    auto *a = stack.PushLayer(CreateScope<TestLayer>("LayerA"));
    stack.PushLayer(CreateScope<TestLayer>("LayerB"));
    stack.PushOverlay(CreateScope<TestLayer>("OverlayA"));

    stack.PopLayer(a);

    std::vector<std::string> names;
    for (const auto &layer : stack)
        names.push_back(layer->GetName());

    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "LayerB");
    EXPECT_EQ(names[1], "OverlayA");
}

TEST(LayerStackTests, PopOverlayRemovesOnlyOverlay)
{
    TestLayer::ResetCounters();

    LayerStack stack;

    stack.PushLayer(CreateScope<TestLayer>("LayerA"));
    auto *overlay = stack.PushOverlay(CreateScope<TestLayer>("OverlayA"));

    stack.PopOverlay(overlay);

    std::vector<std::string> names;
    for (const auto &layer : stack)
        names.push_back(layer->GetName());

    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "LayerA");
}