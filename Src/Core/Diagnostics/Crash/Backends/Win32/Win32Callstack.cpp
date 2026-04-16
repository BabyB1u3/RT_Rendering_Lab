#include "Core/Diagnostics/Crash/Callstack.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>

#include <Windows.h>
#include <DbgHelp.h>

namespace
{
std::mutex g_SymbolMutex;
bool g_SymbolsInitialized = false;
bool g_SymbolsAvailable = false;
} // namespace

namespace Diagnostics
{

std::string CaptureCallstack(int framesToSkip, int maxFrames)
{
    constexpr USHORT kMaxSupportedFrames = 62;
    void* frames[kMaxSupportedFrames]{};
    const USHORT frameCount =
        CaptureStackBackTrace(static_cast<DWORD>(std::max(framesToSkip + 1, 0)),
                              static_cast<DWORD>(std::min(maxFrames, static_cast<int>(kMaxSupportedFrames))),
                              frames,
                              nullptr);

    if (frameCount == 0)
        return "  <callstack unavailable>";

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    HANDLE process = GetCurrentProcess();
    std::lock_guard<std::mutex> lock(g_SymbolMutex);
    if (!g_SymbolsInitialized)
    {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        g_SymbolsAvailable = SymInitialize(process, nullptr, TRUE) == TRUE;
        g_SymbolsInitialized = true;
    }

    for (USHORT i = 0; i < frameCount; ++i)
    {
        const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);
        if (g_SymbolsAvailable)
        {
            DWORD64 displacement = 0;
            std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> symbolStorage{};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage.data());
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            IMAGEHLP_MODULE64 moduleInfo{};
            moduleInfo.SizeOfStruct = sizeof(moduleInfo);
            const bool hasModule = SymGetModuleInfo64(process, static_cast<DWORD64>(address), &moduleInfo) == TRUE;
            const bool hasSymbol = SymFromAddr(process, static_cast<DWORD64>(address), &displacement, symbol) == TRUE;

            if (hasSymbol)
            {
                const std::string moduleName = (hasModule && moduleInfo.ImageName != nullptr)
                                                   ? std::filesystem::path(moduleInfo.ImageName).filename().string()
                                                   : "<unknown>";

                stream << "  [" << i << "] " << moduleName << '!' << symbol->Name << " + 0x" << displacement << " (0x"
                       << std::setw(sizeof(std::uintptr_t) * 2) << address << ")\n";
                continue;
            }
        }

        stream << "  [" << i << "] 0x" << std::setw(sizeof(std::uintptr_t) * 2) << address << '\n';
    }
    return stream.str();
}

} // namespace Diagnostics
