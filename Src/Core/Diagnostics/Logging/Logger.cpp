#include "Core/Diagnostics/Logging/Logger.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "Core/Diagnostics/Logging/FrameFormatter.h"
#include "Core/Diagnostics/Logging/ImGuiConsoleSink.h"
#include "Core/Diagnostics/Logging/JsonLineSink.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Resource/FileSystem.h"

namespace Diagnostics
{

std::vector<spdlog::sink_ptr> Logger::s_Sinks;
Ref<ImGuiConsoleSink> Logger::s_ConsoleSink;
Ref<JsonLineSink> Logger::s_JsonSink;
bool Logger::s_JsonSinkEnabled = false;
std::filesystem::path Logger::s_JsonFilePath;
spdlog::level::level_enum Logger::s_GlobalLevel = spdlog::level::trace;

namespace
{
std::mutex g_LoggerMutex;
bool g_Initialized = false;

std::filesystem::path g_LogFilePath;
constexpr size_t kMaxLogFileSizeBytes = 1024 * 1024;
constexpr size_t kMaxLogFiles = 3;
constexpr std::string_view kDefaultLogVirtualPath = "/Saved/logs/RTRLab.log";

std::filesystem::path ResolveWritableDiagnosticsPath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        if (const auto resolved = FileSystem::ResolveWritePath(kDefaultLogVirtualPath))
            return *resolved;

        return std::filesystem::path("logs") / "RTRLab.log";
    }

    const auto genericPath = path.generic_string();
    if (!genericPath.empty() && FileSystem::IsVirtualPath(genericPath))
    {
        if (const auto resolved = FileSystem::ResolveWritePath(genericPath))
            return *resolved;

        return {};
    }

    return path;
}

std::filesystem::path DeriveJsonLogPath(const std::filesystem::path& logFilePath)
{
    if (logFilePath.empty())
        return {};

    auto jsonPath = logFilePath;
    jsonPath.replace_extension(".jsonl");
    return jsonPath;
}
} // namespace

void Logger::Init(const std::filesystem::path& logFilePath)
{
    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    if (g_Initialized)
        return;

    g_LogFilePath = ResolveWritableDiagnosticsPath(logFilePath);
    if (g_LogFilePath.empty())
        return;

    std::filesystem::create_directories(g_LogFilePath.parent_path());

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        g_LogFilePath.string(), kMaxLogFileSizeBytes, kMaxLogFiles);
    auto fileFormatter = std::make_unique<spdlog::pattern_formatter>();
    fileFormatter->add_flag<FrameFlag>('@');
    fileFormatter->set_pattern("[%Y-%m-%d %T.%e] [%t] [F%@] [%n] [%l] %v");
    fileSink->set_formatter(std::move(fileFormatter));

    s_ConsoleSink = std::make_shared<ImGuiConsoleSink>();
    s_JsonSink = std::make_shared<JsonLineSink>();

    s_Sinks = {consoleSink, fileSink, s_ConsoleSink, s_JsonSink};
    g_Initialized = true;

    spdlog::init_thread_pool(8192, 1);

    auto coreLogger = std::make_shared<spdlog::async_logger>(LogCategory::Core,
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
    spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) { logger->flush(); });
}

void Logger::Shutdown()
{
    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    if (!g_Initialized)
        return;

    // spdlog::shutdown() synchronously drains the async queue, flushes all
    // sinks, stops the thread pool and periodic flusher, then drops all
    // loggers.  It must run BEFORE we close any sink files or release sink
    // pointers, otherwise queued messages could be lost.
    spdlog::shutdown();

    if (s_JsonSink)
        s_JsonSink->Disable();
    s_JsonSink.reset();
    s_JsonSinkEnabled = false;
    s_JsonFilePath.clear();
    s_ConsoleSink.reset();
    s_Sinks.clear();
    s_GlobalLevel = spdlog::level::trace;
    g_LogFilePath.clear();
    g_Initialized = false;
}

Ref<spdlog::logger> Logger::GetLogger(const char* category)
{
    if (!category)
        return {};

    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    if (!g_Initialized)
        return {};

    if (auto existing = spdlog::get(category))
        return existing;

    auto logger = std::make_shared<spdlog::async_logger>(
        category, s_Sinks.begin(), s_Sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
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

bool Logger::HasLogger(const char* category)
{
    if (!category)
        return false;

    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    return g_Initialized && spdlog::get(category) != nullptr;
}

void Logger::SetLevel(const char* category, spdlog::level::level_enum level)
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

    spdlog::apply_all([level](const std::shared_ptr<spdlog::logger>& logger) { logger->set_level(level); });
}

Ref<ImGuiConsoleSink> Logger::GetConsoleSink()
{
    return s_ConsoleSink;
}

bool Logger::IsJsonSinkEnabled()
{
    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    return g_Initialized && s_JsonSinkEnabled;
}

std::filesystem::path Logger::GetDefaultJsonLogPath()
{
    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    return DeriveJsonLogPath(g_LogFilePath);
}

std::filesystem::path Logger::GetJsonSinkPath()
{
    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    return s_JsonFilePath;
}

void Logger::EnableJsonSink(const std::filesystem::path& filePath)
{
    Ref<JsonLineSink> jsonSink;
    std::filesystem::path resolvedPath;
    uint64_t controlGeneration = 0;
    bool shouldLogFailure = false;

    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Initialized || !s_JsonSink)
            return;

        resolvedPath = filePath.empty() ? DeriveJsonLogPath(g_LogFilePath) : ResolveWritableDiagnosticsPath(filePath);
        if (resolvedPath.empty())
            return;

        jsonSink = s_JsonSink;

        if (s_JsonSinkEnabled)
        {
            if (s_JsonFilePath == resolvedPath)
                return;

            controlGeneration = s_JsonSink->RequestReopen(resolvedPath);
            spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) { logger->flush(); });
        }
        else
        {
            s_JsonSinkEnabled = s_JsonSink->Enable(resolvedPath);
            s_JsonFilePath = s_JsonSinkEnabled ? resolvedPath : std::filesystem::path{};
            shouldLogFailure = !s_JsonSinkEnabled;
        }
    }

    if (controlGeneration != 0 && jsonSink)
    {
        jsonSink->WaitForControl(controlGeneration);

        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (s_JsonSink == jsonSink)
        {
            s_JsonSinkEnabled = s_JsonSink->IsEnabled();
            s_JsonFilePath = s_JsonSinkEnabled ? resolvedPath : std::filesystem::path{};
            shouldLogFailure = !s_JsonSinkEnabled;
        }
    }

    if (shouldLogFailure)
    {
        if (auto logger = Logger::GetLogger(LogCategory::Error))
            logger->error("Failed to enable JSON log sink at '{}'", resolvedPath.string());
    }
}

void Logger::DisableJsonSink()
{
    Ref<JsonLineSink> jsonSink;
    uint64_t controlGeneration = 0;

    {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!s_JsonSink || !s_JsonSinkEnabled)
            return;

        jsonSink = s_JsonSink;
        controlGeneration = s_JsonSink->RequestDisable();
        spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) { logger->flush(); });
    }

    if (controlGeneration != 0 && jsonSink)
        jsonSink->WaitForControl(controlGeneration);

    std::lock_guard<std::mutex> lock(g_LoggerMutex);
    if (s_JsonSink == jsonSink)
    {
        s_JsonSinkEnabled = s_JsonSink->IsEnabled();
        if (!s_JsonSinkEnabled)
            s_JsonFilePath.clear();
    }
}

double GetMonotonicSeconds()
{
    using clock = std::chrono::steady_clock;
    static const auto s_Start = clock::now();
    return std::chrono::duration<double>(clock::now() - s_Start).count();
}

} // namespace Diagnostics
