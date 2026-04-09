#include "Core/Diagnostics/Crash/CrashHandler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include "Core/Diagnostics/Logging/Logger.h"

namespace
{
    std::atomic_bool g_CrashHandlerInitialized = false;
    std::string g_SignalCrashPath;
    std::string g_LogPath;
    stack_t g_AlternateStack{};
    std::unique_ptr<std::byte[]> g_AlternateStackStorage;

    constexpr size_t kAlternateStackSize = 64 * 1024;

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

    size_t AppendLiteral(char *buffer, size_t capacity, size_t offset, const char *text)
    {
        if (!text || capacity == 0)
            return offset;

        while (*text != '\0' && offset < capacity)
            buffer[offset++] = *text++;

        return offset;
    }

    size_t AppendUnsigned(char *buffer, size_t capacity, size_t offset, unsigned int value)
    {
        char digits[16];
        size_t digitCount = 0;

        do
        {
            digits[digitCount++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        } while (value != 0 && digitCount < sizeof(digits));

        while (digitCount > 0 && offset < capacity)
            buffer[offset++] = digits[--digitCount];

        return offset;
    }

    size_t AppendPointer(char *buffer, size_t capacity, size_t offset, const void *pointer)
    {
        constexpr char kHexDigits[] = "0123456789ABCDEF";

        offset = AppendLiteral(buffer, capacity, offset, "0x");

        std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
        bool started = false;
        for (int shift = static_cast<int>(sizeof(std::uintptr_t) * 8) - 4; shift >= 0; shift -= 4)
        {
            const unsigned int nibble = static_cast<unsigned int>((value >> shift) & 0xF);
            if (!started && nibble == 0 && shift > 0)
                continue;

            started = true;
            if (offset < capacity)
                buffer[offset++] = kHexDigits[nibble];
        }

        if (!started && offset < capacity)
            buffer[offset++] = '0';

        return offset;
    }

    void SafeWriteAll(const int fd, const char *data, size_t size)
    {
        if (fd < 0 || !data || size == 0)
            return;

        while (size > 0)
        {
            const ssize_t written = ::write(fd, data, size);
            if (written <= 0)
                return;

            data += static_cast<size_t>(written);
            size -= static_cast<size_t>(written);
        }
    }

    void WriteCrashBanner(const int fd, const int signal, const void *address)
    {
        char buffer[256];
        size_t offset = 0;

        offset = AppendLiteral(buffer, sizeof(buffer), offset, "RTRLab POSIX crash\nSignal: ");
        offset = AppendUnsigned(buffer, sizeof(buffer), offset, static_cast<unsigned int>(signal));
        offset = AppendLiteral(buffer, sizeof(buffer), offset, " (");
        offset = AppendLiteral(buffer, sizeof(buffer), offset, SignalName(signal));
        offset = AppendLiteral(buffer, sizeof(buffer), offset, ")\nAddress: ");
        offset = AppendPointer(buffer, sizeof(buffer), offset, address);
        offset = AppendLiteral(buffer, sizeof(buffer), offset, "\n");

        if (!g_LogPath.empty())
        {
            offset = AppendLiteral(buffer, sizeof(buffer), offset, "Log file: ");
            offset = AppendLiteral(buffer, sizeof(buffer), offset, g_LogPath.c_str());
            offset = AppendLiteral(buffer, sizeof(buffer), offset, "\n");
        }

        offset = AppendLiteral(buffer, sizeof(buffer), offset, "\nCallstack:\n");
        SafeWriteAll(fd, buffer, offset);
    }

    int OpenSignalCrashFile()
    {
        if (g_SignalCrashPath.empty())
            return -1;

        return ::open(g_SignalCrashPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
}

namespace Diagnostics
{

    void CrashHandler::Init()
    {
        bool expected = false;
        if (!g_CrashHandlerInitialized.compare_exchange_strong(expected, true))
            return;

        g_SignalCrashPath = (CrashHandler::GetCrashDirectory() / "RTRLab_posix_signal_crash.txt").string();
        g_LogPath = Logger::GetLogFilePath().string();

        g_AlternateStackStorage = std::make_unique<std::byte[]>(kAlternateStackSize);
        g_AlternateStack.ss_sp = g_AlternateStackStorage.get();
        g_AlternateStack.ss_size = kAlternateStackSize;
        g_AlternateStack.ss_flags = 0;
        sigaltstack(&g_AlternateStack, nullptr);

        // Warm up libgcc's backtrace path before any fatal signal arrives.
        void *warmupFrame = nullptr;
        (void)::backtrace(&warmupFrame, 1);

        struct sigaction action{};
        action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
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
        const int crashFd = OpenSignalCrashFile();

        WriteCrashBanner(STDERR_FILENO, signal, address);
        if (crashFd >= 0)
            WriteCrashBanner(crashFd, signal, address);

        void *frames[64];
        const int frameCount = ::backtrace(frames, static_cast<int>(sizeof(frames) / sizeof(frames[0])));
        if (frameCount > 0)
        {
            ::backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);
            SafeWriteAll(STDERR_FILENO, "\n", 1);

            if (crashFd >= 0)
            {
                ::backtrace_symbols_fd(frames, frameCount, crashFd);
                SafeWriteAll(crashFd, "\n", 1);
                ::fsync(crashFd);
                ::close(crashFd);
            }
        }
        else if (crashFd >= 0)
        {
            SafeWriteAll(crashFd, "  <callstack unavailable>\n", 25);
            ::fsync(crashFd);
            ::close(crashFd);
        }

        ::signal(signal, SIG_DFL);
        ::kill(::getpid(), signal);
        _exit(128 + signal);
    }

} // namespace Diagnostics
