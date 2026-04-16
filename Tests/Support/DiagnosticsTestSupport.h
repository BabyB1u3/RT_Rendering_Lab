#pragma once

#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

#include "Core/Util/Base.h"
#include "Core/Diagnostics/Logging/ImGuiConsoleSink.h"
#include "Core/Diagnostics/Logging/Logger.h"
#include "TestPaths.h"

namespace DiagnosticsTestSupport
{
inline std::filesystem::path BaseRoot()
{
    return test_support::CategoryRoot("diagnostics");
}

inline std::filesystem::path TestRoot()
{
    return test_support::CurrentTestRoot("diagnostics");
}

inline std::filesystem::path TestPath(std::string_view relativePath)
{
    return test_support::CurrentTestPath("diagnostics", relativePath);
}

inline void RemoveCurrentTestArtifacts()
{
    test_support::RemoveCurrentTestArtifacts("diagnostics");
}

inline size_t CountEntriesWithMessage(const std::vector<Diagnostics::ConsoleLogEntry>& entries, const char* message)
{
    size_t count = 0;
    for (const auto& entry : entries)
    {
        if (entry.message == message)
            ++count;
    }

    return count;
}

inline size_t CountEntriesContainingMessage(const std::vector<Diagnostics::ConsoleLogEntry>& entries,
                                            const char* messageFragment)
{
    size_t count = 0;
    for (const auto& entry : entries)
    {
        if (entry.message.find(messageFragment) != std::string::npos)
            ++count;
    }

    return count;
}

inline size_t CountEntriesForCategory(const std::vector<Diagnostics::ConsoleLogEntry>& entries, const char* category)
{
    size_t count = 0;
    for (const auto& entry : entries)
    {
        if (entry.category == category)
            ++count;
    }

    return count;
}

inline std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntries(const Ref<Diagnostics::ImGuiConsoleSink>& sink,
                                                                       const char* message,
                                                                       size_t minCount,
                                                                       int timeoutMs = 500,
                                                                       int quietWindowMs = 40)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<Diagnostics::ConsoleLogEntry> entries;
    size_t lastCount = 0;
    auto stableSince = std::chrono::steady_clock::time_point{};

    while (std::chrono::steady_clock::now() < deadline)
    {
        Diagnostics::Logger::Flush();
        entries = sink->GetEntries();
        const size_t currentCount = CountEntriesWithMessage(entries, message);

        if (currentCount != lastCount)
        {
            lastCount = currentCount;
            stableSince = std::chrono::steady_clock::now();
        }
        else if (currentCount >= minCount && stableSince != std::chrono::steady_clock::time_point{} &&
                 (std::chrono::steady_clock::now() - stableSince) >= std::chrono::milliseconds(quietWindowMs))
        {
            return entries;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    Diagnostics::Logger::Flush();
    entries = sink->GetEntries();
    return entries;
}

inline std::vector<Diagnostics::ConsoleLogEntry>
WaitForSettledEntriesContaining(const Ref<Diagnostics::ImGuiConsoleSink>& sink,
                                const char* messageFragment,
                                size_t minCount,
                                int timeoutMs = 500,
                                int quietWindowMs = 40)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<Diagnostics::ConsoleLogEntry> entries;
    size_t lastCount = 0;
    auto stableSince = std::chrono::steady_clock::time_point{};

    while (std::chrono::steady_clock::now() < deadline)
    {
        Diagnostics::Logger::Flush();
        entries = sink->GetEntries();
        const size_t currentCount = CountEntriesContainingMessage(entries, messageFragment);

        if (currentCount != lastCount)
        {
            lastCount = currentCount;
            stableSince = std::chrono::steady_clock::now();
        }
        else if (currentCount >= minCount && stableSince != std::chrono::steady_clock::time_point{} &&
                 (std::chrono::steady_clock::now() - stableSince) >= std::chrono::milliseconds(quietWindowMs))
        {
            return entries;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    Diagnostics::Logger::Flush();
    entries = sink->GetEntries();
    return entries;
}

inline std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntryCount(
    const Ref<Diagnostics::ImGuiConsoleSink>& sink, size_t minCount, int timeoutMs = 1000, int quietWindowMs = 80)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<Diagnostics::ConsoleLogEntry> entries;
    size_t lastCount = 0;
    auto stableSince = std::chrono::steady_clock::time_point{};

    while (std::chrono::steady_clock::now() < deadline)
    {
        Diagnostics::Logger::Flush();
        entries = sink->GetEntries();
        const size_t currentCount = entries.size();

        if (currentCount != lastCount)
        {
            lastCount = currentCount;
            stableSince = std::chrono::steady_clock::now();
        }
        else if (currentCount >= minCount && stableSince != std::chrono::steady_clock::time_point{} &&
                 (std::chrono::steady_clock::now() - stableSince) >= std::chrono::milliseconds(quietWindowMs))
        {
            return entries;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    Diagnostics::Logger::Flush();
    entries = sink->GetEntries();
    return entries;
}
} // namespace DiagnosticsTestSupport
