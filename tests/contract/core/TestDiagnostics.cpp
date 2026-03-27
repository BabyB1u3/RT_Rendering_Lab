#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include <json.hpp>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/Callstack.h"
#include "core/diagnostics/CrashHandler.h"
#include "core/diagnostics/ErrorMacros.h"
#include "core/diagnostics/ImGuiConsoleSink.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"

#include "gui/panels/ConsolePanel.h"

namespace
{
    const std::filesystem::path &ContractTestLogPath()
    {
        static const auto path = FileSystem::GetSavedPath("logs/diagnostics-contract.log");
        return path;
    }

    const std::filesystem::path &ContractTestJsonLogPath()
    {
        static const auto path = FileSystem::GetSavedPath("logs/diagnostics-contract.jsonl");
        return path;
    }

    void RemovePathIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    bool ReturnsFalseWhenErrFailTriggers(bool shouldFail)
    {
        ERR_FAIL_COND_V_MSG_CAT(LogCategory::Core, shouldFail, false, "diagnostics contract test");
        return true;
    }

    void EmitWarnOnceContractMessage()
    {
        LOG_WARN_ONCE_CAT(LogCategory::Core, "warn-once-contract");
    }

    void EmitWarnThrottleContractMessage(double intervalSeconds)
    {
        LOG_WARN_THROTTLE_CAT(LogCategory::Core, intervalSeconds, "warn-throttle-contract");
    }

    bool EmitEnsureOnceContractMessage()
    {
        return RTRLAB_ENSURE_MSG(false, "ensure-once-contract");
    }

    size_t CountEntriesWithMessage(const std::vector<Diagnostics::ConsoleLogEntry> &entries, const char *message)
    {
        size_t count = 0;
        for (const auto &entry : entries)
        {
            if (entry.Message == message)
                ++count;
        }

        return count;
    }

    size_t CountEntriesContainingMessage(const std::vector<Diagnostics::ConsoleLogEntry> &entries, const char *messageFragment)
    {
        size_t count = 0;
        for (const auto &entry : entries)
        {
            if (entry.Message.find(messageFragment) != std::string::npos)
                ++count;
        }

        return count;
    }

    /// Poll the async logger until the matching entry count stays unchanged for
    /// a short quiet window, then return the final snapshot. This is stronger
    /// than "wait until first match", because it can catch duplicate messages
    /// that arrive a little later from the async queue.
    std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntries(
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

    std::vector<Diagnostics::ConsoleLogEntry> WaitForSettledEntriesContaining(
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
}

TEST(DiagnosticsContractTests, LoggerWritesIntoSavedLogsDirectory)
{
    FileSystem::Init();

    const auto &logPath = ContractTestLogPath();
    RemovePathIfExists(logPath);

    Diagnostics::Logger::Init(logPath);
    LOG_INFO_CAT(LogCategory::Core, "diagnostics contract test");
    Diagnostics::Logger::Flush();
    Diagnostics::Logger::Shutdown();

    EXPECT_TRUE(std::filesystem::exists(logPath));

    RemovePathIfExists(logPath);
}

TEST(DiagnosticsContractTests, CrashHandlerInitIsIdempotent)
{
    FileSystem::Init();
    Diagnostics::Logger::Init(ContractTestLogPath());

    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());
    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());

    Diagnostics::Logger::Shutdown();
    RemovePathIfExists(ContractTestLogPath());
}

TEST(DiagnosticsContractTests, CrashHandlerUsesSavedLogsCrashDirectory)
{
    FileSystem::Init();

    const auto expectedPath = FileSystem::GetSavedPath("logs/crashes");
    EXPECT_EQ(Diagnostics::CrashHandler::GetCrashDirectory(), expectedPath);
}

TEST(DiagnosticsContractTests, EnsureIsNonFatalAndReturnsBooleanStatus)
{
    EXPECT_TRUE(RTRLAB_ENSURE(true));
    EXPECT_FALSE(RTRLAB_ENSURE(false));
    EXPECT_FALSE(RTRLAB_ENSURE_MSG(false, "diagnostics ensure contract"));
}

TEST(DiagnosticsContractTests, ErrorMacrosReturnExpectedFallbackValue)
{
    EXPECT_TRUE(ReturnsFalseWhenErrFailTriggers(false));
    EXPECT_FALSE(ReturnsFalseWhenErrFailTriggers(true));
}

TEST(DiagnosticsContractTests, CallstackCaptureIsImplementedOnSupportedPlatforms)
{
    const std::string callstack = Diagnostics::CaptureCallstack();

    EXPECT_FALSE(callstack.empty());

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    EXPECT_EQ(callstack.find("not implemented"), std::string::npos);
#endif
}

TEST(DiagnosticsContractTests, JsonSinkWritesStructuredJsonLinesWhenEnabled)
{
    FileSystem::Init();

    const auto &logPath = ContractTestLogPath();
    const auto &jsonPath = ContractTestJsonLogPath();
    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);

    Diagnostics::Logger::Init(logPath);
    Diagnostics::Logger::EnableJsonSink(jsonPath);
    LOG_INFO_CAT(LogCategory::Core, "json-contract-message");
    Diagnostics::Logger::Flush();
    Diagnostics::Logger::DisableJsonSink();
    Diagnostics::Logger::Shutdown();

    ASSERT_TRUE(std::filesystem::exists(jsonPath));

    std::ifstream input(jsonPath);
    ASSERT_TRUE(input.is_open());

    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    ASSERT_FALSE(line.empty());

    const auto parsed = nlohmann::json::parse(line);
    EXPECT_EQ(parsed.at("cat"), "Core");
    EXPECT_EQ(parsed.at("lvl"), "info");
    EXPECT_EQ(parsed.at("msg"), "json-contract-message");
    EXPECT_TRUE(parsed.contains("ts"));
    EXPECT_TRUE(parsed.contains("frame"));
    EXPECT_TRUE(parsed.contains("tid"));

    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);
}

TEST(DiagnosticsContractTests, JsonSinkStopsWritingAfterDisable)
{
    FileSystem::Init();

    const auto &logPath = ContractTestLogPath();
    const auto &jsonPath = ContractTestJsonLogPath();
    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);

    Diagnostics::Logger::Init(logPath);
    Diagnostics::Logger::EnableJsonSink(jsonPath);
    LOG_INFO_CAT(LogCategory::Core, "json-before-disable");
    Diagnostics::Logger::Flush();
    Diagnostics::Logger::DisableJsonSink();

    LOG_INFO_CAT(LogCategory::Core, "json-after-disable");
    Diagnostics::Logger::Flush();
    Diagnostics::Logger::Shutdown();

    std::ifstream input(jsonPath);
    ASSERT_TRUE(input.is_open());
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-before-disable"), std::string::npos);
    EXPECT_EQ(contents.find("json-after-disable"), std::string::npos);

    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);
}

TEST(DiagnosticsContractTests, ShutdownFlushesEnabledJsonSinkWithoutExplicitDisable)
{
    FileSystem::Init();

    const auto &logPath = ContractTestLogPath();
    const auto &jsonPath = ContractTestJsonLogPath();
    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);

    Diagnostics::Logger::Init(logPath);
    Diagnostics::Logger::EnableJsonSink(jsonPath);
    LOG_INFO_CAT(LogCategory::Core, "json-survives-shutdown");
    Diagnostics::Logger::Shutdown();

    std::ifstream input(jsonPath);
    ASSERT_TRUE(input.is_open());
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-survives-shutdown"), std::string::npos);

    RemovePathIfExists(logPath);
    RemovePathIfExists(jsonPath);
}

TEST(DiagnosticsContractTests, LoggerShutdownIsIdempotent)
{
    FileSystem::Init();

    Diagnostics::Logger::Init(ContractTestLogPath());
    EXPECT_NO_THROW(Diagnostics::Logger::Shutdown());
    EXPECT_NO_THROW(Diagnostics::Logger::Shutdown());

    RemovePathIfExists(ContractTestLogPath());
    RemovePathIfExists(ContractTestJsonLogPath());
}

// --- ImGuiConsoleSink tests ---

class ImGuiConsoleSinkTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_Sink = std::make_shared<Diagnostics::ImGuiConsoleSink>();
    }

    void LogMessage(const std::string &category, spdlog::level::level_enum level, const std::string &msg)
    {
        auto logger = std::make_shared<spdlog::logger>(category, m_Sink);
        logger->set_level(spdlog::level::trace);
        logger->log(level, msg);
    }

    std::shared_ptr<Diagnostics::ImGuiConsoleSink> m_Sink;
};

TEST_F(ImGuiConsoleSinkTests, EntriesAreEmptyByDefault)
{
    EXPECT_TRUE(m_Sink->GetEntries().empty());
}

TEST_F(ImGuiConsoleSinkTests, SinkCapturesLogMessages)
{
    LogMessage("Core", spdlog::level::info, "hello world");

    auto entries = m_Sink->GetEntries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].Category, "Core");
    EXPECT_EQ(entries[0].Level, spdlog::level::info);
    EXPECT_EQ(entries[0].Message, "hello world");
    EXPECT_FALSE(entries[0].Timestamp.empty());
}

TEST_F(ImGuiConsoleSinkTests, ClearRemovesAllEntries)
{
    LogMessage("Core", spdlog::level::info, "msg1");
    LogMessage("Core", spdlog::level::warn, "msg2");
    ASSERT_EQ(m_Sink->GetEntries().size(), 2u);

    m_Sink->Clear();
    EXPECT_TRUE(m_Sink->GetEntries().empty());
}

TEST_F(ImGuiConsoleSinkTests, RingBufferEvictsOldestWhenFull)
{
    // The ring buffer limit is 1024. Write 1030 messages and verify
    // only the newest 1024 remain.
    for (int i = 0; i < 1030; ++i)
        LogMessage("Core", spdlog::level::trace, "msg" + std::to_string(i));

    auto entries = m_Sink->GetEntries();
    EXPECT_EQ(entries.size(), 1024u);
    // The oldest surviving entry should be msg6 (indices 0-5 evicted).
    EXPECT_EQ(entries.front().Message, "msg6");
    EXPECT_EQ(entries.back().Message, "msg1029");
}

TEST_F(ImGuiConsoleSinkTests, MultipleCategoriesAreCaptured)
{
    LogMessage("Shader", spdlog::level::err, "compile failed");
    LogMessage("Window", spdlog::level::info, "created");

    auto entries = m_Sink->GetEntries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].Category, "Shader");
    EXPECT_EQ(entries[1].Category, "Window");
}

TEST_F(ImGuiConsoleSinkTests, ConsoleSinkIsRegisteredByLogger)
{
    FileSystem::Init();
    const auto &logPath = ContractTestLogPath();
    Diagnostics::Logger::Init(logPath);

    auto sink = Diagnostics::Logger::GetConsoleSink();
    EXPECT_NE(sink, nullptr);

    Diagnostics::Logger::Shutdown();
    RemovePathIfExists(logPath);
}

// --- Logger::HasLogger tests ---

class LoggerHasLoggerTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FileSystem::Init();
        Diagnostics::Logger::Init(ContractTestLogPath());
    }

    void TearDown() override
    {
        Diagnostics::Logger::Shutdown();
        RemovePathIfExists(ContractTestLogPath());
    }
};

TEST_F(LoggerHasLoggerTests, ReturnsTrueForCategoryThatHasBeenUsed)
{
    // "Core" is created during Logger::Init, so it should exist.
    EXPECT_TRUE(Diagnostics::Logger::HasLogger(LogCategory::Core));
}

TEST_F(LoggerHasLoggerTests, ReturnsTrueForDynamicCategoryAfterFirstLog)
{
    // A dynamic category doesn't exist until first use.
    EXPECT_FALSE(Diagnostics::Logger::HasLogger("MyPlugin"));

    // Trigger lazy creation via GetLogger (same path as LOG_*_CAT).
    Diagnostics::Logger::GetLogger("MyPlugin");
    EXPECT_TRUE(Diagnostics::Logger::HasLogger("MyPlugin"));
}

TEST_F(LoggerHasLoggerTests, ReturnsFalseForUnknownCategory)
{
    EXPECT_FALSE(Diagnostics::Logger::HasLogger("TotallyFakeCategory"));
}

TEST_F(LoggerHasLoggerTests, DoesNotCreateLoggerAsSideEffect)
{
    EXPECT_FALSE(Diagnostics::Logger::HasLogger("Ghost"));
    // Call again: still false, proving no lazy creation happened.
    EXPECT_FALSE(Diagnostics::Logger::HasLogger("Ghost"));
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandAcceptsKnownCategoryBeforeFirstUse)
{
    EXPECT_FALSE(Diagnostics::Logger::HasLogger(LogCategory::Demo));

    ConsolePanel panel;
    panel.ExecuteCommand("log.level Demo warn");

    EXPECT_TRUE(Diagnostics::Logger::HasLogger(LogCategory::Demo));
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandRejectsUnknownCategoryWithoutCreatingLogger)
{
    ConsolePanel panel;
    panel.ExecuteCommand("log.level Ghost warn");

    EXPECT_FALSE(Diagnostics::Logger::HasLogger("Ghost"));
}

TEST_F(LoggerHasLoggerTests, ConsoleFilterAcceptsExistingDynamicCategory)
{
    Diagnostics::Logger::GetLogger("MyPlugin");

    ConsolePanel panel;
    panel.ExecuteCommand("log.filter MyPlugin");

    EXPECT_EQ(panel.GetCategoryFilterIndex(), 0);
    EXPECT_EQ(panel.GetCommandCategoryFilter(), "MyPlugin");
}

TEST_F(LoggerHasLoggerTests, WarnOnceMacroSuppressesSecondInvocationAtSameCallSite)
{
    auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    EmitWarnOnceContractMessage();
    EmitWarnOnceContractMessage();

    const auto entries = WaitForSettledEntries(sink, "warn-once-contract", 1);
    EXPECT_EQ(CountEntriesWithMessage(entries, "warn-once-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, WarnThrottleMacroSuppressesImmediateSecondInvocation)
{
    auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    EmitWarnThrottleContractMessage(60.0);
    EmitWarnThrottleContractMessage(60.0);

    const auto entries = WaitForSettledEntries(sink, "warn-throttle-contract", 1);
    EXPECT_EQ(CountEntriesWithMessage(entries, "warn-throttle-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, EnsureMacroReportsOnlyOnceAcrossConcurrentCallers)
{
    auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    std::atomic<bool> start{false};
    std::vector<std::thread> workers;
    workers.reserve(8);

    for (int i = 0; i < 8; ++i)
    {
        workers.emplace_back([&start]()
                             {
                                 while (!start.load(std::memory_order_acquire))
                                     std::this_thread::yield();

                                 for (int attempt = 0; attempt < 32; ++attempt)
                                     EXPECT_FALSE(EmitEnsureOnceContractMessage());
                             });
    }

    start.store(true, std::memory_order_release);

    for (auto &worker : workers)
        worker.join();

    const auto entries = WaitForSettledEntriesContaining(sink, "ensure-once-contract", 1, 1000, 80);
    EXPECT_EQ(CountEntriesContainingMessage(entries, "ensure-once-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandCanEnableAndDisableJsonSink)
{
    const auto &jsonPath = ContractTestJsonLogPath();
    RemovePathIfExists(jsonPath);

    ConsolePanel panel;
    panel.ExecuteCommand("log.json on");
    EXPECT_TRUE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_EQ(Diagnostics::Logger::GetJsonSinkPath(), Diagnostics::Logger::GetDefaultJsonLogPath());

    panel.ExecuteCommand("log.json off");
    EXPECT_FALSE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_TRUE(Diagnostics::Logger::GetJsonSinkPath().empty());

    RemovePathIfExists(jsonPath);
}

TEST_F(LoggerHasLoggerTests, LoggerCanSwitchJsonSinkPathsAtRuntime)
{
    const auto firstPath = FileSystem::GetSavedPath("logs/diagnostics-contract-first.jsonl");
    const auto secondPath = FileSystem::GetSavedPath("logs/diagnostics-contract-second.jsonl");
    RemovePathIfExists(firstPath);
    RemovePathIfExists(secondPath);

    Diagnostics::Logger::EnableJsonSink(firstPath);
    LOG_INFO_CAT(LogCategory::Core, "json-first-path");
    Diagnostics::Logger::EnableJsonSink(secondPath);
    LOG_INFO_CAT(LogCategory::Core, "json-second-path");
    Diagnostics::Logger::Shutdown();

    std::ifstream firstInput(firstPath);
    ASSERT_TRUE(firstInput.is_open());
    std::string firstContents((std::istreambuf_iterator<char>(firstInput)), std::istreambuf_iterator<char>());
    EXPECT_NE(firstContents.find("json-first-path"), std::string::npos);
    EXPECT_EQ(firstContents.find("json-second-path"), std::string::npos);

    std::ifstream secondInput(secondPath);
    ASSERT_TRUE(secondInput.is_open());
    std::string secondContents((std::istreambuf_iterator<char>(secondInput)), std::istreambuf_iterator<char>());
    EXPECT_EQ(secondContents.find("json-first-path"), std::string::npos);
    EXPECT_NE(secondContents.find("json-second-path"), std::string::npos);

    RemovePathIfExists(firstPath);
    RemovePathIfExists(secondPath);
}

TEST(LogCategoriesTests, IsKnownCategoryRecognizesPredefinedNames)
{
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Core));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Demo));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Crash));
}

TEST(LogCategoriesTests, IsKnownCategoryRejectsUnknownNames)
{
    EXPECT_FALSE(LogCategory::IsKnownCategory("MyPlugin"));
    EXPECT_FALSE(LogCategory::IsKnownCategory("DefinitelyNotReal"));
}
