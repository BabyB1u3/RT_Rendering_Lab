#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

#include "Core/Diagnostics/Assert.h"
#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Diagnostics/Logger.h"
#include "Core/Resource/FileSystem.h"

#include "GUI/Panels/ConsolePanel.h"

#include "DiagnosticsTestSupport.h"

namespace
{
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

    bool EmitEnsureIndependentSiteA()
    {
        return RTRLAB_ENSURE_MSG(false, "ensure-independent-site-a");
    }

    bool EmitEnsureIndependentSiteB()
    {
        return RTRLAB_ENSURE_MSG(false, "ensure-independent-site-b");
    }
}

class LoggerHasLoggerTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FileSystem::Init();
        Diagnostics::Logger::Init(DiagnosticsTestSupport::TestPath("diagnostics-contract.log"));
    }

    void TearDown() override
    {
        Diagnostics::Logger::Shutdown();
        DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
    }
};

TEST_F(LoggerHasLoggerTests, ReturnsTrueForCategoryThatHasBeenUsed)
{
    EXPECT_TRUE(Diagnostics::Logger::HasLogger(LogCategory::Core));
}

TEST_F(LoggerHasLoggerTests, ReturnsTrueForDynamicCategoryAfterFirstLog)
{
    EXPECT_FALSE(Diagnostics::Logger::HasLogger("MyPlugin"));

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
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    EmitWarnOnceContractMessage();
    EmitWarnOnceContractMessage();

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntries(sink, "warn-once-contract", 1);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "warn-once-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, WarnThrottleMacroSuppressesImmediateSecondInvocation)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    EmitWarnThrottleContractMessage(60.0);
    EmitWarnThrottleContractMessage(60.0);

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntries(sink, "warn-throttle-contract", 1);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "warn-throttle-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, EnsureMacroReportsOnlyOnceAcrossConcurrentCallers)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
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
                                     EXPECT_FALSE(EmitEnsureOnceContractMessage()); });
    }

    start.store(true, std::memory_order_release);

    for (auto &worker : workers)
        worker.join();

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntriesContaining(sink, "ensure-once-contract", 1, 1000, 80);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesContainingMessage(entries, "ensure-once-contract"), 1u);
}

TEST_F(LoggerHasLoggerTests, EnsureMacrosReportIndependentlyPerCallSite)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    EXPECT_FALSE(EmitEnsureIndependentSiteA());
    EXPECT_FALSE(EmitEnsureIndependentSiteA());
    EXPECT_FALSE(EmitEnsureIndependentSiteB());
    EXPECT_FALSE(EmitEnsureIndependentSiteB());

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntryCount(sink, 2);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesContainingMessage(entries, "ensure-independent-site-a"), 1u);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesContainingMessage(entries, "ensure-independent-site-b"), 1u);
}

TEST_F(LoggerHasLoggerTests, CategoryLevelSuppressesMessagesBelowThreshold)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    Diagnostics::Logger::SetLevel(LogCategory::Core, spdlog::level::warn);
    LOG_INFO_CAT(LogCategory::Core, "core-info-filtered");
    LOG_WARN_CAT(LogCategory::Core, "core-warn-visible");

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntries(sink, "core-warn-visible", 1);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "core-info-filtered"), 0u);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "core-warn-visible"), 1u);
}

TEST_F(LoggerHasLoggerTests, GlobalLevelAppliesToExistingAndFutureLoggers)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    Diagnostics::Logger::SetGlobalLevel(spdlog::level::warn);

    LOG_INFO_CAT(LogCategory::Core, "global-info-core-filtered");
    LOG_WARN_CAT(LogCategory::Core, "global-warn-core-visible");

    const auto pluginLogger = Diagnostics::Logger::GetLogger("MyPlugin");
    ASSERT_NE(pluginLogger, nullptr);
    pluginLogger->info("global-info-plugin-filtered");
    pluginLogger->warn("global-warn-plugin-visible");

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntryCount(sink, 2);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "global-info-core-filtered"), 0u);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "global-info-plugin-filtered"), 0u);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "global-warn-core-visible"), 1u);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "global-warn-plugin-visible"), 1u);
}

TEST_F(LoggerHasLoggerTests, ShutdownResetsGlobalLevelForNextInit)
{
    Diagnostics::Logger::SetGlobalLevel(spdlog::level::warn);
    Diagnostics::Logger::Shutdown();
    Diagnostics::Logger::Init(DiagnosticsTestSupport::TestPath("diagnostics-contract.log"));

    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    LOG_INFO_CAT(LogCategory::Core, "info-after-reinit");

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntries(sink, "info-after-reinit", 1);
    EXPECT_EQ(DiagnosticsTestSupport::CountEntriesWithMessage(entries, "info-after-reinit"), 1u);
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandCanEnableAndDisableJsonSink)
{
    const auto jsonPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(jsonPath);

    ConsolePanel panel;
    panel.ExecuteCommand("log.json on");
    EXPECT_TRUE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_EQ(Diagnostics::Logger::GetJsonSinkPath(), Diagnostics::Logger::GetDefaultJsonLogPath());

    panel.ExecuteCommand("log.json off");
    EXPECT_FALSE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_TRUE(Diagnostics::Logger::GetJsonSinkPath().empty());

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandCanEnableJsonSinkAtLogicalSavedPath)
{
    const auto logicalPath = std::string("/Saved/Logs/console-contract.jsonl");
    const auto resolvedPath = FileSystem::ResolveWritePath(logicalPath);
    ASSERT_TRUE(resolvedPath.has_value());
    DiagnosticsTestSupport::RemovePathIfExists(*resolvedPath);

    ConsolePanel panel;
    panel.ExecuteCommand("log.json on \"" + logicalPath + "\"");
    EXPECT_TRUE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_EQ(Diagnostics::Logger::GetJsonSinkPath(), *resolvedPath);

    LOG_INFO_CAT(LogCategory::Core, "json-logical-path");
    Diagnostics::Logger::Shutdown();

    std::ifstream input(*resolvedPath);
    ASSERT_TRUE(input.is_open());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-logical-path"), std::string::npos);

    DiagnosticsTestSupport::RemovePathIfExists(*resolvedPath);
}

TEST_F(LoggerHasLoggerTests, ConsoleCommandCanEnableJsonSinkAtPathWithSpaces)
{
    const auto jsonPath = DiagnosticsTestSupport::TestPath("diagnostics contract spaced path.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(jsonPath);

    ConsolePanel panel;
    panel.ExecuteCommand("log.json on \"" + jsonPath.string() + "\"");
    EXPECT_TRUE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_EQ(Diagnostics::Logger::GetJsonSinkPath(), jsonPath);

    LOG_INFO_CAT(LogCategory::Core, "json-space-path");
    Diagnostics::Logger::Shutdown();

    std::ifstream input(jsonPath);
    ASSERT_TRUE(input.is_open());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("json-space-path"), std::string::npos);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST_F(LoggerHasLoggerTests, LoggerInitWithoutExplicitPathUsesSavedLogsDirectory)
{
    Diagnostics::Logger::Shutdown();

    const auto expectedPath = FileSystem::ResolveWritePath("/Saved/logs/RTRLab.log");
    ASSERT_TRUE(expectedPath.has_value());
    DiagnosticsTestSupport::RemovePathIfExists(*expectedPath);

    Diagnostics::Logger::Init();
    EXPECT_EQ(Diagnostics::Logger::GetLogFilePath(), *expectedPath);

    LOG_INFO_CAT(LogCategory::Core, "default-logical-log-path");
    Diagnostics::Logger::Shutdown();

    EXPECT_TRUE(std::filesystem::exists(*expectedPath));
    DiagnosticsTestSupport::RemovePathIfExists(*expectedPath);
}

TEST_F(LoggerHasLoggerTests, LoggerCanSwitchJsonSinkPathsAtRuntime)
{
    const auto firstPath = DiagnosticsTestSupport::TestPath("diagnostics-contract-first.jsonl");
    const auto secondPath = DiagnosticsTestSupport::TestPath("diagnostics-contract-second.jsonl");
    DiagnosticsTestSupport::RemovePathIfExists(firstPath);
    DiagnosticsTestSupport::RemovePathIfExists(secondPath);

    Diagnostics::Logger::EnableJsonSink(firstPath);
    LOG_INFO_CAT(LogCategory::Core, "json-first-path");
    Diagnostics::Logger::EnableJsonSink(secondPath);
    LOG_INFO_CAT(LogCategory::Core, "json-second-path");
    Diagnostics::Logger::Shutdown();

    std::ifstream firstInput(firstPath);
    ASSERT_TRUE(firstInput.is_open());
    const std::string firstContents((std::istreambuf_iterator<char>(firstInput)), std::istreambuf_iterator<char>());
    EXPECT_NE(firstContents.find("json-first-path"), std::string::npos);
    EXPECT_EQ(firstContents.find("json-second-path"), std::string::npos);

    std::ifstream secondInput(secondPath);
    ASSERT_TRUE(secondInput.is_open());
    const std::string secondContents((std::istreambuf_iterator<char>(secondInput)), std::istreambuf_iterator<char>());
    EXPECT_EQ(secondContents.find("json-first-path"), std::string::npos);
    EXPECT_NE(secondContents.find("json-second-path"), std::string::npos);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST_F(LoggerHasLoggerTests, JsonSinkEnableFailureLeavesSinkDisabledAndLogsError)
{
    const auto blockerPath = DiagnosticsTestSupport::TestPath("diagnostics-json-blocker");
    const auto invalidJsonPath = blockerPath / "child.jsonl";
    DiagnosticsTestSupport::RemovePathIfExists(blockerPath);
    DiagnosticsTestSupport::RemovePathIfExists(invalidJsonPath);

    std::filesystem::create_directories(blockerPath.parent_path());
    {
        std::ofstream output(blockerPath);
        ASSERT_TRUE(output.is_open());
        output << "blocker";
    }

    Diagnostics::Logger::EnableJsonSink(invalidJsonPath);
    EXPECT_FALSE(Diagnostics::Logger::IsJsonSinkEnabled());
    EXPECT_TRUE(Diagnostics::Logger::GetJsonSinkPath().empty());

    Diagnostics::Logger::Shutdown();
    std::ifstream input(DiagnosticsTestSupport::TestPath("diagnostics-contract.log"));
    ASSERT_TRUE(input.is_open());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("Failed to enable JSON log sink"), std::string::npos);

    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}

TEST_F(LoggerHasLoggerTests, MultipleCategoriesCanLogConcurrently)
{
    const auto sink = Diagnostics::Logger::GetConsoleSink();
    ASSERT_NE(sink, nullptr);
    sink->Clear();

    const std::array<const char *, 4> categories = {"Core", "Renderer", "Shader", "MyPlugin"};
    constexpr int kMessagesPerCategory = 32;

    std::vector<std::thread> workers;
    workers.reserve(categories.size());
    for (const char *category : categories)
    {
        workers.emplace_back([category]()
                             {
                                 auto logger = Diagnostics::Logger::GetLogger(category);
                                 if (!logger)
                                     return;
                                 for (int i = 0; i < kMessagesPerCategory; ++i)
                                     logger->info("{}-{}", category, i); });
    }

    for (auto &worker : workers)
        worker.join();

    const auto entries = DiagnosticsTestSupport::WaitForSettledEntryCount(sink, categories.size() * kMessagesPerCategory, 1500, 120);
    for (const char *category : categories)
        EXPECT_EQ(DiagnosticsTestSupport::CountEntriesForCategory(entries, category), static_cast<size_t>(kMessagesPerCategory));
}
