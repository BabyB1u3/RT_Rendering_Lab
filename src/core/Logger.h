#pragma once

#include <memory>
#include <spdlog/spdlog.h>

#include "Base.h"

class Logger
{
public:
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
