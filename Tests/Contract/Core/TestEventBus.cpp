#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Core/Event/EventBus.h"

namespace
{
    struct TestEvent
    {
        int Value = 0;
    };

    struct NestedEvent
    {
        std::string Label;
    };
}

TEST(EventBusContractTests, PublishCallsSubscribersInSubscriptionOrder)
{
    EventBus bus;
    std::vector<int> calls;

    auto first = bus.Subscribe<TestEvent>([&](const TestEvent &event)
                                          { calls.push_back(event.Value + 1); });
    auto second = bus.Subscribe<TestEvent>([&](const TestEvent &event)
                                           { calls.push_back(event.Value + 2); });

    bus.Publish(TestEvent{5});

    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0], 6);
    EXPECT_EQ(calls[1], 7);

    (void)first;
    (void)second;
}

TEST(EventBusContractTests, ScopedConnectionDestructorUnsubscribesHandler)
{
    EventBus bus;
    int callCount = 0;

    {
        auto connection = bus.Subscribe<TestEvent>([&](const TestEvent &)
                                                   { ++callCount; });
        bus.Publish(TestEvent{});
        EXPECT_EQ(callCount, 1);
    }

    bus.Publish(TestEvent{});
    EXPECT_EQ(callCount, 1);
}

TEST(EventBusContractTests, DisconnectDuringDispatchDefersRemovalSafely)
{
    EventBus bus;
    std::vector<int> calls;

    ScopedConnection secondConnection;
    auto firstConnection = bus.Subscribe<TestEvent>([&](const TestEvent &event)
                                                    {
                                                        calls.push_back(event.Value);
                                                        secondConnection.Disconnect(); });
    secondConnection = bus.Subscribe<TestEvent>([&](const TestEvent &event)
                                                { calls.push_back(event.Value * 10); });

    bus.Publish(TestEvent{2});
    bus.Publish(TestEvent{3});

    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0], 2);
    EXPECT_EQ(calls[1], 3);

    (void)firstConnection;
}

TEST(EventBusContractTests, NestedPublishKeepsSubscriberListsStable)
{
    EventBus bus;
    std::vector<std::string> calls;

    auto nested = bus.Subscribe<NestedEvent>([&](const NestedEvent &event)
                                             { calls.push_back("nested:" + event.Label); });
    auto root = bus.Subscribe<TestEvent>([&](const TestEvent &event)
                                         {
                                             calls.push_back("root:" + std::to_string(event.Value));
                                             bus.Publish(NestedEvent{"inner"});
                                             calls.push_back("root:end"); });

    bus.Publish(TestEvent{42});

    ASSERT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[0], "root:42");
    EXPECT_EQ(calls[1], "nested:inner");
    EXPECT_EQ(calls[2], "root:end");

    (void)nested;
    (void)root;
}
