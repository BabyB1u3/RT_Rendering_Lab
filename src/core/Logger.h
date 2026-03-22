#pragma once

/// @file Logger.h
/// @brief Application-wide logging via spdlog.
///
/// Call Logger::Init() once at startup to create the console logger.
/// Then use the LOG_* macros (TRACE / INFO / WARN / ERROR / CRITICAL).
///
/// All macros are null-safe: if called before Init() they silently no-op,
/// so early startup code and static initializers never crash.

#include <memory>
#include <spdlog/spdlog.h>

#include "Base.h"

class Logger
{
public:
    /// Create the console logger with colored output. Call once at startup.
    static void Init();
    static Ref<spdlog::logger> &GetLogger() { return s_Logger; }

private:
    static Ref<spdlog::logger> s_Logger;
};

#define LOG_TRACE(...)                              \
    do                                              \
    {                                               \
        const auto &logger = ::Logger::GetLogger(); \
        if (logger)                                 \
            logger->trace(__VA_ARGS__);             \
    } while (0)

#define LOG_INFO(...)                               \
    do                                              \
    {                                               \
        const auto &logger = ::Logger::GetLogger(); \
        if (logger)                                 \
            logger->info(__VA_ARGS__);              \
    } while (0)

#define LOG_WARN(...)                               \
    do                                              \
    {                                               \
        const auto &logger = ::Logger::GetLogger(); \
        if (logger)                                 \
            logger->warn(__VA_ARGS__);              \
    } while (0)

#define LOG_ERROR(...)                              \
    do                                              \
    {                                               \
        const auto &logger = ::Logger::GetLogger(); \
        if (logger)                                 \
            logger->error(__VA_ARGS__);             \
    } while (0)

#define LOG_CRITICAL(...)                           \
    do                                              \
    {                                               \
        const auto &logger = ::Logger::GetLogger(); \
        if (logger)                                 \
            logger->critical(__VA_ARGS__);          \
    } while (0)
