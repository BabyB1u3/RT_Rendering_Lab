#include "core/diagnostics/CrashHandler.h"

#include <atomic>
#include <cstring>

#include <spdlog/fmt/fmt.h>

namespace
{
    std::atomic_bool g_CrashHandlerInitialized = false;

    const char *SignalName(int signal)
    {
        switch (signal)
        {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGABRT:
            return "SIGABRT";
        case SIGFPE:
            return "SIGFPE";
        case SIGBUS:
            return "SIGBUS";
        case SIGILL:
            return "SIGILL";
        default:
            return "UNKNOWN";
        }
    }
}

namespace Diagnostics
{

    void CrashHandler::Init()
    {
        bool expected = false;
        if (!g_CrashHandlerInitialized.compare_exchange_strong(expected, true))
            return;

        struct sigaction action{};
        action.sa_flags = SA_SIGINFO | SA_RESETHAND;
        action.sa_sigaction = &CrashHandler::PosixSignalHandler;
        sigemptyset(&action.sa_mask);

        sigaction(SIGSEGV, &action, nullptr);
        sigaction(SIGABRT, &action, nullptr);
        sigaction(SIGFPE, &action, nullptr);
        sigaction(SIGBUS, &action, nullptr);
        sigaction(SIGILL, &action, nullptr);
    }

    void CrashHandler::PosixSignalHandler(int signal, siginfo_t *info, void * /*context*/)
    {
        const void *address = info ? info->si_addr : nullptr;
        const auto reason = fmt::format("Received signal {} ({}) at {}", signal, SignalName(signal), address);
        Detail::HandleCrash(reason.c_str(), {}, nullptr);
    }

} // namespace Diagnostics
