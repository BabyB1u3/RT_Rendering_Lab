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

#include "core/event/ScopedConnection.h"

class EventBus
{
public:
    /// Subscribe to events of type T. Returns an RAII handle that
    /// auto-unsubscribes on destruction.
    template <typename T>
    ScopedConnection Subscribe(std::function<void(const T &)> handler)
    {
        auto &subs = GetSubscribers<T>();
        uint64_t id = m_NextId++;

        subs.Entries.push_back({id, std::move(handler), false});

        return ScopedConnection([this, id]()
                                {
            auto& s = GetSubscribers<T>();
            for (auto& entry : s.Entries)
            {
                if (entry.Id == id)
                {
                    entry.PendingRemoval = true;
                    s.Dirty = true;
                    break;
                }
            } });
    }

    /// Publish an event. All subscribers of type T are called synchronously.
    /// Safe to subscribe/unsubscribe from within a handler.
    template <typename T>
    void Publish(const T &event)
    {
        auto &subs = GetSubscribers<T>();
        subs.DispatchDepth++;

        for (size_t i = 0; i < subs.Entries.size(); ++i)
        {
            auto &entry = subs.Entries[i];
            if (!entry.PendingRemoval)
                entry.Handler(event);
        }

        subs.DispatchDepth--;

        // Compact removed entries only when all nested dispatches are done.
        if (subs.DispatchDepth == 0 && subs.Dirty)
        {
            subs.Entries.erase(
                std::remove_if(subs.Entries.begin(), subs.Entries.end(),
                               [](const auto &e)
                               { return e.PendingRemoval; }),
                subs.Entries.end());
            subs.Dirty = false;
        }
    }

private:
    struct ISubscriberList
    {
        virtual ~ISubscriberList() = default;
    };

    template <typename T>
    struct SubscriberEntry
    {
        uint64_t Id;
        std::function<void(const T &)> Handler;
        bool PendingRemoval = false;
    };

    template <typename T>
    struct SubscriberList : ISubscriberList
    {
        std::vector<SubscriberEntry<T>> Entries;
        int DispatchDepth = 0;
        bool Dirty = false;
    };

    template <typename T>
    SubscriberList<T> &GetSubscribers()
    {
        auto key = std::type_index(typeid(T));
        auto it = m_Subscribers.find(key);
        if (it == m_Subscribers.end())
        {
            auto list = std::make_unique<SubscriberList<T>>();
            auto *ptr = list.get();
            m_Subscribers[key] = std::move(list);
            return *ptr;
        }
        return *static_cast<SubscriberList<T> *>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<ISubscriberList>> m_Subscribers;
    uint64_t m_NextId = 0;
};
