#pragma once

/// @file LogMacros.h
/// @brief Category-aware logging macros with compile-time level stripping.
///
/// Compile-time level stripping via RTRLAB_LOG_MIN_LEVEL:
///   0 = Trace (default Debug), 1 = Debug, 2 = Info (default Release),
///   3 = Warn, 4 = Error, 5 = Critical, 6 = Off
/// Macros below the min level expand to ((void)0) - zero runtime cost.

#include <atomic>

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/Logger.h"

#ifndef RTRLAB_LOG_MIN_LEVEL
#ifdef NDEBUG
#define RTRLAB_LOG_MIN_LEVEL 2
#else
#define RTRLAB_LOG_MIN_LEVEL 0
#endif
#endif

// --- Helper: the actual log call (used by guarded macros below) ---

#define RTRLAB_LOG_IMPL_(category, level, ...)                     \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->level(__VA_ARGS__);                           \
    } while (0)

// --- Category-aware macros with compile-time stripping ---
//   0 = Trace, 1 = Debug, 2 = Info, 3 = Warn, 4 = Error, 5 = Critical, 6 = Off

#if RTRLAB_LOG_MIN_LEVEL <= 0
#define LOG_TRACE_CAT(category, ...) RTRLAB_LOG_IMPL_(category, trace, __VA_ARGS__)
#else
#define LOG_TRACE_CAT(category, ...) ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 1
#define LOG_DEBUG_CAT(category, ...) RTRLAB_LOG_IMPL_(category, debug, __VA_ARGS__)
#else
#define LOG_DEBUG_CAT(category, ...) ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 2
#define LOG_INFO_CAT(category, ...) RTRLAB_LOG_IMPL_(category, info, __VA_ARGS__)
#else
#define LOG_INFO_CAT(category, ...) ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 3
#define LOG_WARN_CAT(category, ...) RTRLAB_LOG_IMPL_(category, warn, __VA_ARGS__)
#else
#define LOG_WARN_CAT(category, ...) ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 4
#define LOG_ERROR_CAT(category, ...) RTRLAB_LOG_IMPL_(category, error, __VA_ARGS__)
#else
#define LOG_ERROR_CAT(category, ...) ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 5
#define LOG_CRITICAL_CAT(category, ...) RTRLAB_LOG_IMPL_(category, critical, __VA_ARGS__)
#else
#define LOG_CRITICAL_CAT(category, ...) ((void)0)
#endif

// --- LOG_*_ONCE: log only on the FIRST occurrence at each call site ---

#define LOG_WARN_ONCE_CAT(category, ...)                                \
    do                                                                  \
    {                                                                   \
        static std::atomic<bool> _logged{false};                        \
        bool _expected = false;                                         \
        if (_logged.compare_exchange_strong(_expected, true,            \
                                            std::memory_order_relaxed)) \
        {                                                               \
            LOG_WARN_CAT(category, __VA_ARGS__);                        \
        }                                                               \
    } while (0)

#define LOG_ERROR_ONCE_CAT(category, ...)                               \
    do                                                                  \
    {                                                                   \
        static std::atomic<bool> _logged{false};                        \
        bool _expected = false;                                         \
        if (_logged.compare_exchange_strong(_expected, true,            \
                                            std::memory_order_relaxed)) \
        {                                                               \
            LOG_ERROR_CAT(category, __VA_ARGS__);                       \
        }                                                               \
    } while (0)

// --- LOG_*_THROTTLE: log at most once per intervalSeconds at each call site ---

#define LOG_WARN_THROTTLE_CAT(category, intervalSeconds, ...)                  \
    do                                                                         \
    {                                                                          \
        static std::atomic<double> _lastLogTime{-1e9};                         \
        double _now = ::Diagnostics::GetMonotonicSeconds();                    \
        double _previous = _lastLogTime.load(std::memory_order_relaxed);       \
        while ((_now - _previous) >= (intervalSeconds))                        \
        {                                                                      \
            if (_lastLogTime.compare_exchange_weak(_previous, _now,            \
                                                   std::memory_order_relaxed)) \
            {                                                                  \
                LOG_WARN_CAT(category, __VA_ARGS__);                           \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

#define LOG_ERROR_THROTTLE_CAT(category, intervalSeconds, ...)                 \
    do                                                                         \
    {                                                                          \
        static std::atomic<double> _lastLogTime{-1e9};                         \
        double _now = ::Diagnostics::GetMonotonicSeconds();                    \
        double _previous = _lastLogTime.load(std::memory_order_relaxed);       \
        while ((_now - _previous) >= (intervalSeconds))                        \
        {                                                                      \
            if (_lastLogTime.compare_exchange_weak(_previous, _now,            \
                                                   std::memory_order_relaxed)) \
            {                                                                  \
                LOG_ERROR_CAT(category, __VA_ARGS__);                          \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

// --- LOG_*_COND: log only when condition is true ---

#define LOG_WARN_COND_CAT(condition, category, ...) \
    do                                              \
    {                                               \
        if (condition) [[unlikely]]                 \
        {                                           \
            LOG_WARN_CAT(category, __VA_ARGS__);    \
        }                                           \
    } while (0)

#define LOG_ERROR_COND_CAT(condition, category, ...) \
    do                                               \
    {                                                \
        if (condition) [[unlikely]]                  \
        {                                            \
            LOG_ERROR_CAT(category, __VA_ARGS__);    \
        }                                            \
    } while (0)
