#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>

#include "Core/Resource/FileSystem.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#ifdef RTRLAB_LOG_MIN_LEVEL
#undef RTRLAB_LOG_MIN_LEVEL
#endif
#define RTRLAB_LOG_MIN_LEVEL 3
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Diagnostics/Logging/Logger.h"
#include "TestPaths.h"

namespace
{
    std::filesystem::path MinLevelContractTestLogPath()
    {
        const auto path = FileSystem::ResolveWritePath("/Saved/logs/diagnostics-min-level-contract.log");
        return path.value_or(std::filesystem::path{});
    }

    std::atomic<int> g_SideEffectCounter{0};

    int NextSideEffectValue()
    {
        return ++g_SideEffectCounter;
    }
}

class DiagnosticsMinLevelContractTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FileSystem::Init();
        test_support::RemovePathIfExists(MinLevelContractTestLogPath());
        Diagnostics::Logger::Init(MinLevelContractTestLogPath());
        g_SideEffectCounter.store(0, std::memory_order_relaxed);
    }

    void TearDown() override
    {
        Diagnostics::Logger::Shutdown();
        const auto logPath = MinLevelContractTestLogPath();
        test_support::RemovePathIfExists(logPath);
        test_support::RemoveDirectoryIfEmpty(logPath.parent_path());
    }
};

TEST_F(DiagnosticsMinLevelContractTests, InfoAndBelowAreCompiledOutAtWarnThreshold)
{
    LOG_TRACE_CAT(LogCategory::Core, "trace {}", NextSideEffectValue());
    LOG_DEBUG_CAT(LogCategory::Core, "debug {}", NextSideEffectValue());
    LOG_INFO_CAT(LogCategory::Core, "info {}", NextSideEffectValue());

    EXPECT_EQ(g_SideEffectCounter.load(std::memory_order_relaxed), 0);
}

TEST_F(DiagnosticsMinLevelContractTests, WarnAndAboveStillEvaluateArguments)
{
    LOG_WARN_CAT(LogCategory::Core, "warn {}", NextSideEffectValue());
    LOG_ERROR_CAT(LogCategory::Core, "error {}", NextSideEffectValue());
    LOG_CRITICAL_CAT(LogCategory::Core, "critical {}", NextSideEffectValue());

    EXPECT_EQ(g_SideEffectCounter.load(std::memory_order_relaxed), 3);
}
