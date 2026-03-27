#pragma once

/// @file LogMacros.h
/// @brief Category-aware logging macros with backward-compatible legacy aliases.

#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/Logger.h"

#define LOG_TRACE_CAT(category, ...)                               \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->trace(__VA_ARGS__);                           \
    } while (0)

#define LOG_INFO_CAT(category, ...)                                \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->info(__VA_ARGS__);                            \
    } while (0)

#define LOG_WARN_CAT(category, ...)                                \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->warn(__VA_ARGS__);                            \
    } while (0)

#define LOG_ERROR_CAT(category, ...)                               \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->error(__VA_ARGS__);                           \
    } while (0)

#define LOG_CRITICAL_CAT(category, ...)                            \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->critical(__VA_ARGS__);                        \
    } while (0)

#define LOG_TRACE(...) LOG_TRACE_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_INFO(...) LOG_INFO_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_WARN(...) LOG_WARN_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_ERROR(...) LOG_ERROR_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_CRITICAL_CAT(LogCategory::Core, __VA_ARGS__)
