#pragma once

/// @file diagnostics/Logger.h
/// @brief Category-aware diagnostics logger backed by spdlog.

#include <filesystem>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

#include "Core/Util/Base.h"

namespace Diagnostics
{

    class ImGuiConsoleSink;
    class JsonLineSink;

    class Logger
    {
    public:
        static void Init(const std::filesystem::path &logFilePath = {});
        static void Flush();
        static void Shutdown();

        static Ref<spdlog::logger> GetLogger(const char *category);
        static std::filesystem::path GetLogFilePath();

        static bool HasLogger(const char *category);
        static void SetLevel(const char *category, spdlog::level::level_enum level);
        static void SetGlobalLevel(spdlog::level::level_enum level);

        static Ref<ImGuiConsoleSink> GetConsoleSink();

        static bool IsJsonSinkEnabled();
        static std::filesystem::path GetDefaultJsonLogPath();
        static std::filesystem::path GetJsonSinkPath();
        static void EnableJsonSink(const std::filesystem::path &filePath);
        static void DisableJsonSink();

    private:
        static std::vector<spdlog::sink_ptr> s_Sinks;
        static Ref<ImGuiConsoleSink> s_ConsoleSink;
        static Ref<JsonLineSink> s_JsonSink;
        static bool s_JsonSinkEnabled;
        static std::filesystem::path s_JsonFilePath;
        static spdlog::level::level_enum s_GlobalLevel;
    };

    /// Monotonic clock in seconds. Used by throttle macros.
    double GetMonotonicSeconds();

} // namespace Diagnostics
