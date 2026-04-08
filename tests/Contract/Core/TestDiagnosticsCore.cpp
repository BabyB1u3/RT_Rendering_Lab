#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <json.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/base_sink.h>

#include "Core/Resource/FileSystem.h"
#include "Core/Diagnostics/Assert.h"
#include "Core/Diagnostics/Callstack.h"
#include "Core/Diagnostics/CrashHandler.h"
#include "Core/Diagnostics/ErrorMacros.h"
#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Diagnostics/Logger.h"

#include "DiagnosticsTestSupport.h"

namespace
{
    bool ReturnsFalseWhenErrFailTriggers(bool shouldFail)
    {
        ERR_FAIL_COND_V_MSG_CAT(LogCategory::Core, shouldFail, false, "diagnostics contract test");
        return true;
    }

    class SlowCollectingSink : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        std::vector<std::string> Messages()
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            return m_Messages;
        }

    protected:
        void sink_it_(const spdlog::details::log_msg &msg) override
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            m_Messages.emplace_back(msg.payload.data(), msg.payload.size());
        }

        void flush_() override {}

    private:
        std::vector<std::string> m_Messages;
    };
}

TEST(DiagnosticsContractTests, LoggerWritesIntoConfiguredDirectory)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.log");
    DiagnosticsTestSupport::RemovePathIfExists(logPath);

    Diagnostics::Logger::Init(logPath);
    LOG_INFO_CAT(LogCategory::Core, "diagnostics contract test");
    Diagnostics::Logger::Flush();
    Diagnostics::Logger::Shutdown();

    EXPECT_TRUE(std::filesystem::exists(logPath));

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, CrashHandlerInitIsIdempotent)
{
    FileSystem::Init();
    Diagnostics::Logger::Init(DiagnosticsTestSupport::TestPath("diagnostics-contract.log"));

    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());
    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());

    Diagnostics::Logger::Shutdown();
    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
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

#if defined(_WIN32)
    EXPECT_NE(callstack.find('!'), std::string::npos);
#endif
}

TEST(DiagnosticsContractTests, JsonSinkWritesStructuredJsonLinesWhenEnabled)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.log");
    const auto jsonPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(logPath);
    DiagnosticsTestSupport::RemovePathIfExists(jsonPath);

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

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, JsonSinkStopsWritingAfterDisable)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.log");
    const auto jsonPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(logPath);
    DiagnosticsTestSupport::RemovePathIfExists(jsonPath);

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
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-before-disable"), std::string::npos);
    EXPECT_EQ(contents.find("json-after-disable"), std::string::npos);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, ShutdownFlushesEnabledJsonSinkWithoutExplicitDisable)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.log");
    const auto jsonPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(logPath);
    DiagnosticsTestSupport::RemovePathIfExists(jsonPath);

    Diagnostics::Logger::Init(logPath);
    Diagnostics::Logger::EnableJsonSink(jsonPath);
    LOG_INFO_CAT(LogCategory::Core, "json-survives-shutdown");
    Diagnostics::Logger::Shutdown();

    std::ifstream input(jsonPath);
    ASSERT_TRUE(input.is_open());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-survives-shutdown"), std::string::npos);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, LoggerShutdownIsIdempotent)
{
    Diagnostics::Logger::Init(DiagnosticsTestSupport::TestPath("diagnostics-contract.log"));
    EXPECT_NO_THROW(Diagnostics::Logger::Shutdown());
    EXPECT_NO_THROW(Diagnostics::Logger::Shutdown());

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, LoggerRotatesFilesAtConfiguredSize)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-rotation-contract.log");
    const auto rotated1 = logPath.parent_path() / (logPath.stem().string() + ".1" + logPath.extension().string());
    const auto rotated2 = logPath.parent_path() / (logPath.stem().string() + ".2" + logPath.extension().string());
    const auto rotated3 = logPath.parent_path() / (logPath.stem().string() + ".3" + logPath.extension().string());

    DiagnosticsTestSupport::RemovePathIfExists(logPath);
    DiagnosticsTestSupport::RemovePathIfExists(rotated1);
    DiagnosticsTestSupport::RemovePathIfExists(rotated2);
    DiagnosticsTestSupport::RemovePathIfExists(rotated3);

    Diagnostics::Logger::Init(logPath);
    const std::string payload(16 * 1024, 'x');
    for (int i = 0; i < 96; ++i)
        LOG_INFO_CAT(LogCategory::Core, "rotation-contract-{} {}", i, payload);
    Diagnostics::Logger::Shutdown();

    EXPECT_TRUE(std::filesystem::exists(logPath));
    EXPECT_TRUE(std::filesystem::exists(rotated1));

    size_t fileCount = 0;
    for (const auto &entry : std::filesystem::directory_iterator(logPath.parent_path()))
    {
        const auto filename = entry.path().filename().string();
        if (filename.find("diagnostics-rotation-contract") != std::string::npos)
            ++fileCount;
    }

    EXPECT_LE(fileCount, 4u);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST(DiagnosticsContractTests, AsyncOverrunOldestPolicyDropsOldMessagesUnderPressure)
{
    auto sink = std::make_shared<SlowCollectingSink>();
    auto threadPool = std::make_shared<spdlog::details::thread_pool>(8, 1);
    auto logger = std::make_shared<spdlog::async_logger>(
        "overrun-contract",
        sink,
        threadPool,
        spdlog::async_overflow_policy::overrun_oldest);
    logger->set_level(spdlog::level::trace);

    constexpr int kMessageCount = 128;
    for (int i = 0; i < kMessageCount; ++i)
        logger->info("overrun-contract-{}", i);
    logger->flush();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (threadPool->queue_size() != 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_GT(threadPool->overrun_counter(), 0u);
    EXPECT_EQ(threadPool->discard_counter(), 0u);

    const auto messages = sink->Messages();
    ASSERT_FALSE(messages.empty());
    EXPECT_LT(messages.size(), static_cast<size_t>(kMessageCount));
    EXPECT_NE(std::find(messages.begin(), messages.end(), "overrun-contract-127"), messages.end());
}
