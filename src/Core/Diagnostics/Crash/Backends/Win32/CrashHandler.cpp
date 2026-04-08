#include "Core/Diagnostics/Crash/CrashHandler.h"

#include <atomic>

#include <Windows.h>
#include <spdlog/fmt/fmt.h>

namespace
{
    std::atomic_bool g_CrashHandlerInitialized = false;
}

namespace Diagnostics
{

    void CrashHandler::Init()
    {
        bool expected = false;
        if (!g_CrashHandlerInitialized.compare_exchange_strong(expected, true))
            return;

        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        SetUnhandledExceptionFilter(&CrashHandler::UnhandledExceptionHandler);
    }

    long __stdcall CrashHandler::UnhandledExceptionHandler(EXCEPTION_POINTERS *exceptionInfo)
    {
        const DWORD code = (exceptionInfo && exceptionInfo->ExceptionRecord)
                               ? exceptionInfo->ExceptionRecord->ExceptionCode
                               : 0u;
        const void *address = (exceptionInfo && exceptionInfo->ExceptionRecord)
                                  ? exceptionInfo->ExceptionRecord->ExceptionAddress
                                  : nullptr;

        const auto reason = fmt::format("Unhandled SEH exception 0x{:08X} at {}", code, address);
        Detail::HandleCrash(reason.c_str(), {}, exceptionInfo);
    }

} // namespace Diagnostics
