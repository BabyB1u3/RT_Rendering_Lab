#include <gtest/gtest.h>

#include <filesystem>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/Callstack.h"
#include "core/diagnostics/CrashHandler.h"
#include "core/diagnostics/ErrorMacros.h"
#include "core/diagnostics/ImGuiConsoleSink.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"

namespace
{
    const std::filesystem::path &ContractTestLogPath()
    {
        static const auto path = FileSystem::GetSavedPath("logs/diagnostics-contract.log");
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
