#pragma once

/// @file CrashHandler.h
/// @brief Cross-platform crash handling and crash-artifact generation.

#include <filesystem>
#include <string_view>

#ifndef _WIN32
#include <signal.h>
#endif

namespace Diagnostics
{

namespace Detail
{
[[noreturn]] void HandleCrash(const char* reason, std::string_view callstack, const void* nativeContext);
}

class CrashHandler
{
public:
    static void Init();
    static std::filesystem::path GetCrashDirectory();
    [[noreturn]] static void FatalError(const char* reason, std::string_view callstack = {});

private:
#if defined(_WIN32)
    static long __stdcall UnhandledExceptionHandler(struct _EXCEPTION_POINTERS* exceptionInfo);
#else
    static void PosixSignalHandler(int signal, siginfo_t* info, void* context);
#endif
};

} // namespace Diagnostics
