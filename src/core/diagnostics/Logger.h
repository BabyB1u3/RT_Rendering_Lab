#pragma once

/// @file diagnostics/Logger.h
/// @brief Category-aware diagnostics logger backed by spdlog.

#include <filesystem>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

#include "core/Base.h"

namespace Diagnostics
{

    class ImGuiConsoleSink;

    class Logger
    {
    public:
        static void Init(const std::filesystem::path &logFilePath = {});
        static void Flush();
        static void Shutdown();

        static Ref<spdlog::logger> GetLogger(const char *category);
        static std::filesystem::path GetLogFilePath();

        static void SetLevel(const char *category, spdlog::level::level_enum level);
        static void SetGlobalLevel(spdlog::level::level_enum level);

        static Ref<ImGuiConsoleSink> GetConsoleSink();

    private:
        static std::vector<spdlog::sink_ptr> s_Sinks;
        static Ref<ImGuiConsoleSink> s_ConsoleSink;
        static spdlog::level::level_enum s_GlobalLevel;
    };

} // namespace Diagnostics
