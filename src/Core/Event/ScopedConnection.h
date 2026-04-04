#pragma once

/// @file ScopedConnection.h
/// @brief RAII handle for EventBus subscriptions. Destructor auto-unsubscribes.

#include <functional>
#include <memory>

class ScopedConnection
{
public:
    ScopedConnection() = default;

    explicit ScopedConnection(std::function<void()> unsub)
        : m_Unsubscribe(std::make_unique<std::function<void()>>(std::move(unsub)))
    {
    }

    ~ScopedConnection()
    {
        if (m_Unsubscribe && *m_Unsubscribe)
            (*m_Unsubscribe)();
    }

    // Move-only
    ScopedConnection(ScopedConnection &&) = default;
    ScopedConnection &operator=(ScopedConnection &&) = default;

    ScopedConnection(const ScopedConnection &) = delete;
    ScopedConnection &operator=(const ScopedConnection &) = delete;

    void Disconnect()
    {
        if (m_Unsubscribe && *m_Unsubscribe)
        {
            (*m_Unsubscribe)();
            m_Unsubscribe.reset();
        }
    }

private:
    std::unique_ptr<std::function<void()>> m_Unsubscribe;
};
