#include "core/diagnostics/Callstack.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace Diagnostics
{

    std::string CaptureCallstack(int framesToSkip, int maxFrames)
    {
#if defined(_WIN32)
        constexpr USHORT kMaxSupportedFrames = 62;
        void *frames[kMaxSupportedFrames]{};
        const USHORT frameCount = CaptureStackBackTrace(
            static_cast<DWORD>(std::max(framesToSkip + 1, 0)),
            static_cast<DWORD>(std::min(maxFrames, static_cast<int>(kMaxSupportedFrames))),
            frames,
            nullptr);

        if (frameCount == 0)
            return "  <callstack unavailable>";

        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (USHORT i = 0; i < frameCount; ++i)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);
            stream << "  [" << i << "] 0x" << std::setw(sizeof(std::uintptr_t) * 2) << address << '\n';
        }
        return stream.str();
#else
        (void)framesToSkip;
        (void)maxFrames;
        return "  <callstack capture not implemented on this platform>";
#endif
    }

} // namespace Diagnostics
