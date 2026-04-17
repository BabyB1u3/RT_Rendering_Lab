#include <gtest/gtest.h>

#include "Core/Util/Base.h"
#include "Core/App/Layer/LayerStack.h"
#include "LifecycleTrackingLayer.h"

TEST(LayerStackLifecycleTests, PushLayerCallsOnAttachExactlyOnce)
{
    LayerStack stack;
    auto state = std::make_shared<LayerLifecycleState>();

    stack.PushLayer(CreateScope<LifecycleTrackingLayer>("LayerA", state));

    EXPECT_EQ(state->AttachCount, 1);
    EXPECT_EQ(state->DetachCount, 0);
}

TEST(LayerStackLifecycleTests, PushOverlayCallsOnAttachExactlyOnce)
{
    LayerStack stack;
    auto state = std::make_shared<LayerLifecycleState>();

    stack.PushOverlay(CreateScope<LifecycleTrackingLayer>("OverlayA", state));

    EXPECT_EQ(state->AttachCount, 1);
    EXPECT_EQ(state->DetachCount, 0);
}

TEST(LayerStackLifecycleTests, PopLayerCallsOnDetachExactlyOnce)
{
    LayerStack stack;
    auto state = std::make_shared<LayerLifecycleState>();

    Layer* layer = stack.PushLayer(CreateScope<LifecycleTrackingLayer>("LayerA", state));
    stack.PopLayer(layer);

    EXPECT_EQ(state->AttachCount, 1);
    EXPECT_EQ(state->DetachCount, 1);
}

TEST(LayerStackLifecycleTests, PopOverlayCallsOnDetachExactlyOnce)
{
    LayerStack stack;
    auto state = std::make_shared<LayerLifecycleState>();

    Layer* overlay = stack.PushOverlay(CreateScope<LifecycleTrackingLayer>("OverlayA", state));
    stack.PopOverlay(overlay);

    EXPECT_EQ(state->AttachCount, 1);
    EXPECT_EQ(state->DetachCount, 1);
}

TEST(LayerStackLifecycleTests, DestroyingStackDetachesRemainingLayers)
{
    auto layerState = std::make_shared<LayerLifecycleState>();
    auto overlayState = std::make_shared<LayerLifecycleState>();

    {
        LayerStack stack;
        stack.PushLayer(CreateScope<LifecycleTrackingLayer>("LayerA", layerState));
        stack.PushOverlay(CreateScope<LifecycleTrackingLayer>("OverlayA", overlayState));

        EXPECT_EQ(layerState->AttachCount, 1);
        EXPECT_EQ(overlayState->AttachCount, 1);
        EXPECT_EQ(layerState->DetachCount, 0);
        EXPECT_EQ(overlayState->DetachCount, 0);
    }

    EXPECT_EQ(layerState->DetachCount, 1);
    EXPECT_EQ(overlayState->DetachCount, 1);
}

TEST(LayerStackLifecycleTests, PoppingMissingLayerDoesNotDetachExistingLayers)
{
    LayerStack stack;
    auto existingState = std::make_shared<LayerLifecycleState>();
    auto missingState = std::make_shared<LayerLifecycleState>();

    stack.PushLayer(CreateScope<LifecycleTrackingLayer>("LayerA", existingState));
    auto missingLayer = CreateScope<LifecycleTrackingLayer>("MissingLayer", missingState);

    stack.PopLayer(missingLayer.get());

    EXPECT_EQ(existingState->AttachCount, 1);
    EXPECT_EQ(existingState->DetachCount, 0);
    EXPECT_EQ(missingState->AttachCount, 0);
    EXPECT_EQ(missingState->DetachCount, 0);
}
