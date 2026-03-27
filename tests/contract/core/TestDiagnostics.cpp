#include <gtest/gtest.h>

#include <filesystem>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/Callstack.h"
#include "core/diagnostics/CrashHandler.h"
#include "core/diagnostics/ErrorMacros.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"

namespace
{
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

    const auto logPath = FileSystem::GetSavedPath("logs/diagnostics-contract.log");
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
    Diagnostics::Logger::Init(FileSystem::GetSavedPath("logs/diagnostics-contract.log"));

    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());
    EXPECT_NO_THROW(Diagnostics::CrashHandler::Init());

    Diagnostics::Logger::Shutdown();
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
