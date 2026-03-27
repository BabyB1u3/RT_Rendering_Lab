#include "core/diagnostics/Callstack.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>

namespace
{

    std::string DemangleSymbol(const char *symbolName)
    {
        if (!symbolName)
            return "<unknown>";

        int status = 0;
        std::unique_ptr<char, decltype(&std::free)> demangled(
            abi::__cxa_demangle(symbolName, nullptr, nullptr, &status),
            &std::free);

        if (status == 0 && demangled)
            return demangled.get();

        return symbolName;
    }

} // namespace

namespace Diagnostics
{

    std::string CaptureCallstack(int framesToSkip, int maxFrames)
    {
        const int skipCount = std::max(framesToSkip + 1, 0);
        const int frameCapacity = std::max(skipCount + maxFrames, 1);

        std::vector<void *> frames(static_cast<size_t>(frameCapacity));
        const int frameCount = ::backtrace(frames.data(), frameCapacity);
        if (frameCount <= skipCount)
            return "  <callstack unavailable>";

        std::ostringstream stream;
        int logicalIndex = 0;
        for (int i = skipCount; i < frameCount && logicalIndex < maxFrames; ++i, ++logicalIndex)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);

            Dl_info info{};
            if (::dladdr(frames[i], &info) != 0)
            {
                const std::string moduleName = info.dli_fname
                                                   ? std::filesystem::path(info.dli_fname).filename().string()
                                                   : "<unknown>";
                const std::string symbolName = DemangleSymbol(info.dli_sname);

                std::uintptr_t offset = 0;
                if (info.dli_saddr)
                {
                    offset = address - reinterpret_cast<std::uintptr_t>(info.dli_saddr);
                }

                stream << "  [" << logicalIndex << "] "
                       << moduleName << '!' << symbolName
                       << " + 0x" << std::hex << offset
                       << " (0x" << address << ')' << std::dec << '\n';
                continue;
            }

            stream << "  [" << logicalIndex << "] "
                   << "0x" << std::hex << address << std::dec << '\n';
        }

        return stream.str();
    }

} // namespace Diagnostics
