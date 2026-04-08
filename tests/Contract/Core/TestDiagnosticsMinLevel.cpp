#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>

#include "core/FileSystem.h"
#include "core/diagnostics/LogCategories.h"
#ifdef RTRLAB_LOG_MIN_LEVEL
#undef RTRLAB_LOG_MIN_LEVEL
#endif
#define RTRLAB_LOG_MIN_LEVEL 3
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"

namespace
{
    const std::filesystem::path &MinLevelContractTestLogPath()
    {
        static const auto path = FileSystem::GetSavedPath("logs/diagnostics-min-level-contract.log");
        return path;
    }

    void RemovePathIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
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
        RemovePathIfExists(MinLevelContractTestLogPath());
        Diagnostics::Logger::Init(MinLevelContractTestLogPath());
        g_SideEffectCounter.store(0, std::memory_order_relaxed);
    }

    void TearDown() override
    {
        Diagnostics::Logger::Shutdown();
        RemovePathIfExists(MinLevelContractTestLogPath());
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
