#pragma once

#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

#include "core/Base.h"
#include "core/diagnostics/ImGuiConsoleSink.h"
#include "core/diagnostics/Logger.h"

namespace DiagnosticsTestSupport
{
    inline std::filesystem::path BaseRoot()
    {
        return std::filesystem::current_path() / "test-output" / "diagnostics";
    }

    inline std::filesystem::path TestRoot()
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        return BaseRoot() / info->test_suite_name() / info->name();
    }

    inline std::filesystem::path TestPath(std::string_view relativePath)
    {
        return TestRoot() / relativePath;
    }

    inline void RemovePathIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    inline void RemoveCurrentTestArtifacts()
    {
        std::error_code ec;
        std::filesystem::remove_all(TestRoot(), ec);
        ec.clear();
        std::filesystem::remove(BaseRoot().parent_path(), ec);
        ec.clear();
        std::filesystem::remove(BaseRoot(), ec);
    }

    inline size_t CountEntriesWithMessage(const std::vector<Diagnostics::ConsoleLogEntry> &entries, const char *message)
    {
        size_t count = 0;
        for (const auto &entry : entries)
        {
            if (entry.Message == message)
                ++count;
        }

        return count;
    }

    inline size_t CountEntriesContainingMessage(const std::vector<Diagnostics::ConsoleLogEntry> &entries, const char *messageFragment)
    {
        size_t count = 0;
        for (const auto &entry : entries)
        {
            if (entry.Message.find(messageFragment) != std::string::npos)
                ++count;
        }

        return count;
    }

    inline size_t CountEntriesForCategory(const std::vector<Diagnostics::ConsoleLogEntry> &entries, const char *category)
    {
        size_t count = 0;
        for (const auto &entry : entries)
        {
            if (entry.Category == category)
                ++count;
        }

        return count;
    }

    inline std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntries(
        const Ref<Diagnostics::ImGuiConsoleSink> &sink,
        const char *message,
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
            else if (currentCount >= minCount &&
                     stableSince != std::chrono::steady_clock::time_point{} &&
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

    inline std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntriesContaining(
        const Ref<Diagnostics::ImGuiConsoleSink> &sink,
        const char *messageFragment,
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
            else if (currentCount >= minCount &&
                     stableSince != std::chrono::steady_clock::time_point{} &&
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
        const Ref<Diagnostics::ImGuiConsoleSink> &sink,
        size_t minCount,
        int timeoutMs = 1000,
        int quietWindowMs = 80)
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
            else if (currentCount >= minCount &&
                     stableSince != std::chrono::steady_clock::time_point{} &&
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
}
