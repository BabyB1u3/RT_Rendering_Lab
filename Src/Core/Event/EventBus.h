#pragma once

/// @file EventBus.h
/// @brief Lightweight type-safe publish-subscribe event bus.
///
/// Design principles:
///   - Type-safe dispatch: Subscribe<T> receives const T&. No base class.
///   - RAII lifetime: Subscribe returns ScopedConnection that auto-unsubscribes.
///   - No allocation per event: events are stack-constructed, passed by reference.
///   - Safe during dispatch: deferred removal prevents iterator invalidation
///     when handlers unsubscribe mid-Publish.

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>

#include "Core/Event/ScopedConnection.h"

class EventBus
{
public:
    /// Subscribe to events of type T. Returns an RAII handle that
    /// auto-unsubscribes on destruction.
    template <typename T> ScopedConnection Subscribe(std::function<void(const T&)> handler)
    {
        auto& subscribers = GetSubscribers<T>();
        uint64_t id = m_NextId++;

        subscribers.m_Entries.push_back({id, std::move(handler), false});

        return ScopedConnection(
            [this, id]()
            {
                auto& subscribers = GetSubscribers<T>();
                for (auto& entry : subscribers.m_Entries)
                {
                    if (entry.m_Id == id)
                    {
                        entry.m_IsPendingRemoval = true;
                        subscribers.m_IsDirty = true;
                        break;
                    }
                }
            });
    }

    /// Publish an event. All subscribers of type T are called synchronously.
    /// Safe to subscribe/unsubscribe from within a handler.
    template <typename T> void Publish(const T& event)
    {
        auto& subscribers = GetSubscribers<T>();
        subscribers.m_DispatchDepth++;

        for (size_t i = 0; i < subscribers.m_Entries.size(); ++i)
        {
            auto& entry = subscribers.m_Entries[i];
            if (!entry.m_IsPendingRemoval)
                entry.m_Handler(event);
        }

        subscribers.m_DispatchDepth--;

        // Compact removed entries only when all nested dispatches are done.
        if (subscribers.m_DispatchDepth == 0 && subscribers.m_IsDirty)
        {
            subscribers.m_Entries.erase(std::remove_if(subscribers.m_Entries.begin(),
                                                       subscribers.m_Entries.end(),
                                                       [](const auto& entry) { return entry.m_IsPendingRemoval; }),
                                        subscribers.m_Entries.end());
            subscribers.m_IsDirty = false;
        }
    }

private:
    struct SubscriberListBase
    {
        virtual ~SubscriberListBase() = default;
    };

    template <typename T> struct SubscriberEntry
    {
        uint64_t m_Id;
        std::function<void(const T&)> m_Handler;
        bool m_IsPendingRemoval = false;
    };

    template <typename T> struct SubscriberList : SubscriberListBase
    {
        std::vector<SubscriberEntry<T>> m_Entries;
        int m_DispatchDepth = 0;
        bool m_IsDirty = false;
    };

    template <typename T> SubscriberList<T>& GetSubscribers()
    {
        auto key = std::type_index(typeid(T));
        auto it = m_Subscribers.find(key);
        if (it == m_Subscribers.end())
        {
            auto list = std::make_unique<SubscriberList<T>>();
            auto* ptr = list.get();
            m_Subscribers[key] = std::move(list);
            return *ptr;
        }
        return *static_cast<SubscriberList<T>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<SubscriberListBase>> m_Subscribers;
    uint64_t m_NextId = 0;
};
