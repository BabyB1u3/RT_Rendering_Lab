#pragma once

/// @file Assert.h
/// @brief Always-on assertion and ensure macros for engine diagnostics.

#include <spdlog/fmt/fmt.h>

#include "core/diagnostics/Callstack.h"
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"

namespace Diagnostics::Detail
{

    [[noreturn]] void OnAssertionFailed(
        const char *expr,
        const char *file,
        int line,
        const char *func,
        const char *msg);

    void OnEnsureFailed(
        const char *expr,
        const char *file,
        int line,
        const char *func,
        const char *msg);

} // namespace Diagnostics::Detail

#define RTRLAB_ASSERT(condition)                                        \
    do                                                                  \
    {                                                                   \
        if (!(condition)) [[unlikely]]                                  \
        {                                                               \
            ::Diagnostics::Detail::OnAssertionFailed(                   \
                #condition, __FILE__, __LINE__, __FUNCTION__, nullptr); \
        }                                                               \
    } while (0)

#define RTRLAB_ASSERT_MSG(condition, message)                           \
    do                                                                  \
    {                                                                   \
        if (!(condition)) [[unlikely]]                                  \
        {                                                               \
            ::Diagnostics::Detail::OnAssertionFailed(                   \
                #condition, __FILE__, __LINE__, __FUNCTION__, message); \
        }                                                               \
    } while (0)

#define RTRLAB_ASSERTF(condition, ...)                                       \
    do                                                                       \
    {                                                                        \
        if (!(condition)) [[unlikely]]                                       \
        {                                                                    \
            auto _msg = fmt::format(__VA_ARGS__);                            \
            ::Diagnostics::Detail::OnAssertionFailed(                        \
                #condition, __FILE__, __LINE__, __FUNCTION__, _msg.c_str()); \
        }                                                                    \
    } while (0)

#define RTRLAB_VERIFY(expr) \
    ([&]()                  \
     {                                                                         \
         auto _result = (expr);                                                \
         if (!(_result)) [[unlikely]]                                          \
         {                                                                     \
             ::Diagnostics::Detail::OnAssertionFailed(                         \
                 #expr, __FILE__, __LINE__, __FUNCTION__, nullptr);            \
         }                                                                     \
         return _result; }())

#define RTRLAB_ENSURE(condition) \
    ([&]()                       \
     {                                                                         \
         if (!(condition))                                                     \
         {                                                                     \
             static bool _reported = false;                                    \
             if (!_reported)                                                   \
             {                                                                 \
                 _reported = true;                                             \
                 ::Diagnostics::Detail::OnEnsureFailed(                        \
                     #condition, __FILE__, __LINE__, __FUNCTION__, nullptr);   \
             }                                                                 \
             return false;                                                     \
         }                                                                     \
         return true; }())

#define RTRLAB_ENSURE_MSG(condition, message) \
    ([&]()                                    \
     {                                                                         \
         if (!(condition))                                                     \
         {                                                                     \
             static bool _reported = false;                                    \
             if (!_reported)                                                   \
             {                                                                 \
                 _reported = true;                                             \
                 ::Diagnostics::Detail::OnEnsureFailed(                        \
                     #condition, __FILE__, __LINE__, __FUNCTION__, message);   \
             }                                                                 \
             return false;                                                     \
         }                                                                     \
         return true; }())

#define RTRLAB_ENSUREF(condition, ...) \
    ([&]()                             \
     {                                                                         \
         if (!(condition))                                                     \
         {                                                                     \
             static bool _reported = false;                                    \
             if (!_reported)                                                   \
             {                                                                 \
                 _reported = true;                                             \
                 auto _msg = fmt::format(__VA_ARGS__);                         \
                 ::Diagnostics::Detail::OnEnsureFailed(                        \
                     #condition, __FILE__, __LINE__, __FUNCTION__,             \
                     _msg.c_str());                                            \
             }                                                                 \
             return false;                                                     \
         }                                                                     \
         return true; }())
