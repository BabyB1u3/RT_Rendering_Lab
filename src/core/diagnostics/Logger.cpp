#include "core/diagnostics/Logger.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "core/diagnostics/FrameFormatter.h"
#include "core/diagnostics/ImGuiConsoleSink.h"
#include "core/diagnostics/JsonLineSink.h"
#include "core/diagnostics/LogCategories.h"

namespace Diagnostics
{

    std::vector<spdlog::sink_ptr> Logger::s_Sinks;
    Ref<ImGuiConsoleSink> Logger::s_ConsoleSink;
    Ref<JsonLineSink> Logger::s_JsonSink;
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
        consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            g_LogFilePath.string(),
            kMaxLogFileSizeBytes,
            kMaxLogFiles);
        auto fileFormatter = std::make_unique<spdlog::pattern_formatter>();
        fileFormatter->add_flag<FrameFlag>('@');
        fileFormatter->set_pattern("[%Y-%m-%d %T.%e] [%t] [F%@] [%n] [%l] %v");
        fileSink->set_formatter(std::move(fileFormatter));

        s_ConsoleSink = std::make_shared<ImGuiConsoleSink>();
        s_JsonSink = std::make_shared<JsonLineSink>();

        s_Sinks = {consoleSink, fileSink, s_ConsoleSink, s_JsonSink};
        g_Initialized = true;

        spdlog::init_thread_pool(8192, 1);

        auto coreLogger = std::make_shared<spdlog::async_logger>(
            LogCategory::Core,
            s_Sinks.begin(),
            s_Sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);
        coreLogger->set_level(s_GlobalLevel);
        coreLogger->flush_on(spdlog::level::err);
        spdlog::register_logger(coreLogger);

        spdlog::flush_every(std::chrono::seconds(3));
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

        // spdlog::shutdown() synchronously drains the async queue, flushes all
        // sinks, stops the thread pool and periodic flusher, then drops all
        // loggers.  It must run BEFORE we close any sink files or release sink
        // pointers, otherwise queued messages could be lost.
        spdlog::shutdown();

        if (s_JsonSink)
            s_JsonSink->Disable();
        s_JsonSink.reset();
        s_ConsoleSink.reset();
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

        auto logger = std::make_shared<spdlog::async_logger>(
            category,
            s_Sinks.begin(),
            s_Sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);
        logger->set_level(s_GlobalLevel);
        logger->flush_on(spdlog::level::err);
        spdlog::register_logger(logger);
        return logger;
    }

    std::filesystem::path Logger::GetLogFilePath()
    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        return g_LogFilePath;
    }

    bool Logger::HasLogger(const char *category)
    {
        if (!category)
            return false;

        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        return g_Initialized && spdlog::get(category) != nullptr;
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

    Ref<ImGuiConsoleSink> Logger::GetConsoleSink()
    {
        return s_ConsoleSink;
    }

    void Logger::EnableJsonSink(const std::filesystem::path &filePath)
    {
        // s_JsonSink is always in the sink list; Enable() opens the file.
        // Thread-safe: Enable() locks the base_sink mutex internally,
        // which is the same mutex the async backend acquires in sink_it_().
        if (s_JsonSink)
            s_JsonSink->Enable(filePath);
    }

    void Logger::DisableJsonSink()
    {
        // Disable() closes the file under the base_sink mutex.
        if (s_JsonSink)
            s_JsonSink->Disable();
    }

    double GetMonotonicSeconds()
    {
        using clock = std::chrono::steady_clock;
        static const auto s_Start = clock::now();
        return std::chrono::duration<double>(clock::now() - s_Start).count();
    }

} // namespace Diagnostics
