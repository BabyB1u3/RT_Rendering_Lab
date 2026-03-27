#include "core/diagnostics/Logger.h"

#include <filesystem>
#include <mutex>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "core/diagnostics/LogCategories.h"

namespace Diagnostics
{

    std::vector<spdlog::sink_ptr> Logger::s_Sinks;
    spdlog::level::level_enum Logger::s_GlobalLevel = spdlog::level::trace;

    namespace
    {
        std::mutex g_LoggerMutex;
        bool g_Initialized = false;

        std::filesystem::path g_LogFilePath;
        constexpr size_t kMaxLogFileSizeBytes = 1024 * 1024;
        constexpr size_t kMaxLogFiles = 3;
    }

    void Logger::Init(const std::filesystem::path &logFilePath)
    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (g_Initialized)
            return;

        g_LogFilePath = logFilePath.empty() ? (std::filesystem::path("logs") / "RTRLab.log") : logFilePath;
        std::filesystem::create_directories(g_LogFilePath.parent_path());

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            g_LogFilePath.string(),
            kMaxLogFileSizeBytes,
            kMaxLogFiles);

        s_Sinks = {consoleSink, fileSink};
        g_Initialized = true;
        auto coreLogger = CreateRef<spdlog::logger>(
            LogCategory::Core,
            s_Sinks.begin(),
            s_Sinks.end());
        coreLogger->set_level(s_GlobalLevel);
        coreLogger->flush_on(spdlog::level::err);
        coreLogger->set_pattern("[%T] [%n] [%^%l%$] %v");
        spdlog::register_logger(coreLogger);
    }

    void Logger::Flush()
    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        spdlog::apply_all([](const std::shared_ptr<spdlog::logger> &logger)
                          { logger->flush(); });
    }

    void Logger::Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        spdlog::apply_all([](const std::shared_ptr<spdlog::logger> &logger)
                          { logger->flush(); });
        spdlog::drop_all();
        s_Sinks.clear();
        g_LogFilePath.clear();
        g_Initialized = false;
    }

    Ref<spdlog::logger> Logger::GetLogger(const char *category)
    {
        if (!category)
            return {};

        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Initialized)
            return {};

        if (auto existing = spdlog::get(category))
            return existing;

        auto logger = CreateRef<spdlog::logger>(
            category,
            s_Sinks.begin(),
            s_Sinks.end());
        logger->set_level(s_GlobalLevel);
        logger->flush_on(spdlog::level::err);
        logger->set_pattern("[%T] [%n] [%^%l%$] %v");
        spdlog::register_logger(logger);
        return logger;
    }

    void Logger::SetLevel(const char *category, spdlog::level::level_enum level)
    {
        auto logger = GetLogger(category);
        if (!logger)
            return;

        logger->set_level(level);
    }

    void Logger::SetGlobalLevel(spdlog::level::level_enum level)
    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        s_GlobalLevel = level;

        spdlog::apply_all([level](const std::shared_ptr<spdlog::logger> &logger)
                          { logger->set_level(level); });
    }

} // namespace Diagnostics
