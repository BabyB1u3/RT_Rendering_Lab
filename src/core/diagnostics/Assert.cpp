#include "core/diagnostics/Assert.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace Diagnostics::Detail
{

    namespace
    {

        void WriteFallbackMessage(
            const char *kind,
            const char *expr,
            const char *file,
            int line,
            const char *func,
            const char *msg,
            const std::string &callstack)
        {
            if (msg)
            {
                std::fprintf(stderr,
                             "%s failed: %s (%s)\n  in %s (%s:%d)\nCallstack:\n%s\n",
                             kind,
                             expr,
                             msg,
                             func,
                             file,
                             line,
                             callstack.c_str());
            }
            else
            {
                std::fprintf(stderr,
                             "%s failed: %s\n  in %s (%s:%d)\nCallstack:\n%s\n",
                             kind,
                             expr,
                             func,
                             file,
                             line,
                             callstack.c_str());
            }

            std::fflush(stderr);
        }

    } // namespace

    [[noreturn]] void OnAssertionFailed(
        const char *expr,
        const char *file,
        int line,
        const char *func,
        const char *msg)
    {
        const std::string callstack = CaptureCallstack(/*framesToSkip=*/2);
        auto logger = Logger::GetLogger(LogCategory::Assert);
        if (logger)
        {
            if (msg)
            {
                logger->critical("Assertion failed: {} ({})\n  in {} ({}:{})",
                                 expr, msg, func, file, line);
            }
            else
            {
                logger->critical("Assertion failed: {}\n  in {} ({}:{})",
                                 expr, func, file, line);
            }

            logger->critical("Callstack:\n{}", callstack);
        }
        else
        {
            WriteFallbackMessage("Assertion", expr, file, line, func, msg, callstack);
        }
        Logger::Flush();

#if defined(_WIN32)
        if (IsDebuggerPresent())
            __debugbreak();
#endif

        std::abort();
    }

    void OnEnsureFailed(
        const char *expr,
        const char *file,
        int line,
        const char *func,
        const char *msg)
    {
        const std::string callstack = CaptureCallstack(/*framesToSkip=*/2);
        auto logger = Logger::GetLogger(LogCategory::Ensure);
        if (logger)
        {
            if (msg)
            {
                logger->error("Ensure failed: {} ({})\n  in {} ({}:{})",
                              expr, msg, func, file, line);
            }
            else
            {
                logger->error("Ensure failed: {}\n  in {} ({}:{})",
                              expr, func, file, line);
            }

            logger->error("Callstack:\n{}", callstack);
        }
        else
        {
            WriteFallbackMessage("Ensure", expr, file, line, func, msg, callstack);
        }
        Logger::Flush();
    }

} // namespace Diagnostics::Detail
