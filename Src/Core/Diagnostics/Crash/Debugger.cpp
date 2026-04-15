#include "Core/Diagnostics/Crash/Debugger.h"

#include <csignal>
#include <cstdlib>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#endif

namespace Diagnostics
{

bool IsDebuggerAttached()
{
#if defined(_WIN32)
    return ::IsDebuggerPresent() != FALSE;
#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    kinfo_proc info{};
    size_t size = sizeof(info);
    if (::sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
        return false;
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
    {
        constexpr const char* kTracerPid = "TracerPid:";
        if (line.rfind(kTracerPid, 0) == 0)
        {
            const auto value = line.substr(std::char_traits<char>::length(kTracerPid));
            return std::atoi(value.c_str()) != 0;
        }
    }
    return false;
#else
    return false;
#endif
}

void BreakInDebuggerIfAttached()
{
    if (!IsDebuggerAttached())
        return;

#if defined(_WIN32)
    __debugbreak();
#else
    std::raise(SIGTRAP);
#endif
}

} // namespace Diagnostics
