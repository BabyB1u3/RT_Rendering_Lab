#include "Core/Diagnostics/CrashHandler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>

#include "Core/Resource/FileSystem.h"
#include "Core/Diagnostics/Callstack.h"
#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Diagnostics/Logger.h"

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#else
#include <unistd.h>
#endif

namespace
{
    std::atomic_flag g_HandlingCrash = ATOMIC_FLAG_INIT;

    std::string MakeCrashTimestamp()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};

#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &localTime);
        return buffer;
    }

    std::filesystem::path MakeCrashArtifactPath(const std::string &timestamp, const char *extension)
    {
        return Diagnostics::CrashHandler::GetCrashDirectory() / ("RTRLab_" + timestamp + extension);
    }

    std::string ReadLogTail(const std::filesystem::path &logPath, size_t maxLines)
    {
        if (logPath.empty())
            return {};

        std::ifstream input(logPath);
        if (!input.is_open())
            return {};

        std::deque<std::string> lines;
        std::string line;
        while (std::getline(input, line))
        {
            lines.push_back(std::move(line));
            if (lines.size() > maxLines)
                lines.pop_front();
        }

        std::ostringstream tail;
        for (const auto &entry : lines)
            tail << entry << '\n';
        return tail.str();
    }

    void WriteCrashSummary(
        const std::filesystem::path &summaryPath,
        const std::string &timestamp,
        const char *reason,
        const std::string &callstack,
        const std::filesystem::path &dumpPath)
    {
        std::ofstream output(summaryPath, std::ios::out | std::ios::trunc);
        if (!output.is_open())
            return;

        output << "RTRLab Crash Summary\n";
        output << "Timestamp: " << timestamp << "\n";
        output << "Reason: " << (reason ? reason : "Unknown fatal error") << "\n";

        if (!dumpPath.empty())
            output << "Minidump: " << dumpPath.string() << "\n";

        output << "\nCallstack:\n"
               << callstack << "\n";

        const auto logTail = ReadLogTail(Diagnostics::Logger::GetLogFilePath(), 80);
        if (!logTail.empty())
            output << "\nRecent Log Tail:\n"
                   << logTail;
    }

    void WriteFallbackCrashMessage(
        const char *reason,
        const std::string &callstack,
        const std::filesystem::path &summaryPath,
        const std::filesystem::path &dumpPath)
    {
        std::fprintf(stderr, "Fatal error: %s\n", reason ? reason : "Unknown fatal error");
        std::fprintf(stderr, "Crash summary: %s\n", summaryPath.string().c_str());
        if (!dumpPath.empty())
            std::fprintf(stderr, "Minidump: %s\n", dumpPath.string().c_str());
        std::fprintf(stderr, "Callstack:\n%s\n", callstack.c_str());
        std::fflush(stderr);
    }

#ifdef _WIN32
    bool WriteMiniDump(const std::filesystem::path &dumpPath, const void *nativeContext)
    {
        HANDLE fileHandle = CreateFileW(
            dumpPath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE)
            return false;

        auto *exceptionPointers = reinterpret_cast<EXCEPTION_POINTERS *>(const_cast<void *>(nativeContext));

        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = exceptionPointers;
        exceptionInfo.ClientPointers = FALSE;

        const BOOL success = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            fileHandle,
            MiniDumpNormal,
            exceptionPointers ? &exceptionInfo : nullptr,
            nullptr,
            nullptr);

        CloseHandle(fileHandle);
        return success == TRUE;
    }
#endif

    [[noreturn]] void TerminateAfterCrash()
    {
        Diagnostics::Logger::Flush();

#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(), 1);
        std::abort();
#else
        _exit(1);
#endif
    }

} // namespace

namespace Diagnostics::Detail
{

    [[noreturn]] void HandleCrash(const char *reason, std::string_view callstack, const void *nativeContext)
    {
        if (g_HandlingCrash.test_and_set())
            TerminateAfterCrash();

        const std::string resolvedCallstack = callstack.empty()
                                                  ? CaptureCallstack(/*framesToSkip=*/2)
                                                  : std::string(callstack);

        const auto timestamp = MakeCrashTimestamp();
        const auto summaryPath = MakeCrashArtifactPath(timestamp, ".txt");

        std::filesystem::path dumpPath;
#ifdef _WIN32
        const auto candidateDumpPath = MakeCrashArtifactPath(timestamp, ".dmp");
        if (WriteMiniDump(candidateDumpPath, nativeContext))
            dumpPath = candidateDumpPath;
#endif

        auto logger = Logger::GetLogger(LogCategory::Crash);
        if (logger)
        {
            logger->critical("Fatal error: {}", reason ? reason : "Unknown fatal error");
            logger->critical("Callstack:\n{}", resolvedCallstack);
            logger->critical("Crash summary: {}", summaryPath.string());
            if (!dumpPath.empty())
                logger->critical("Minidump: {}", dumpPath.string());
        }
        else
        {
            WriteFallbackCrashMessage(reason, resolvedCallstack, summaryPath, dumpPath);
        }

        WriteCrashSummary(summaryPath, timestamp, reason, resolvedCallstack, dumpPath);
        Logger::Flush();
        TerminateAfterCrash();
    }

} // namespace Diagnostics::Detail

namespace Diagnostics
{

    std::filesystem::path CrashHandler::GetCrashDirectory()
    {
        const auto crashDir = FileSystem::ResolveWritePath("/Saved/logs/crashes");
        if (!crashDir.has_value())
            return {};

        std::error_code ec;
        std::filesystem::create_directories(*crashDir, ec);
        return *crashDir;
    }

    [[noreturn]] void CrashHandler::FatalError(const char *reason, std::string_view callstack)
    {
        Detail::HandleCrash(reason, callstack, nullptr);
    }

} // namespace Diagnostics
