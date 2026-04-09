#pragma once

/// @file ErrorMacros.h
/// @brief Log-and-return helpers for recoverable engine error paths.

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

#define ERR_FAIL_COND_CAT(category, condition)                                                       \
    do                                                                                               \
    {                                                                                                \
        if (condition) [[unlikely]]                                                                  \
        {                                                                                            \
            LOG_ERROR_CAT(category, "{}:{}: condition '{}' failed", __FILE__, __LINE__, #condition); \
            return;                                                                                  \
        }                                                                                            \
    } while (0)

#define ERR_FAIL_COND_V_CAT(category, condition, retval)                                             \
    do                                                                                               \
    {                                                                                                \
        if (condition) [[unlikely]]                                                                  \
        {                                                                                            \
            LOG_ERROR_CAT(category, "{}:{}: condition '{}' failed", __FILE__, __LINE__, #condition); \
            return retval;                                                                           \
        }                                                                                            \
    } while (0)

#define ERR_FAIL_COND_MSG_CAT(category, condition, message)             \
    do                                                                  \
    {                                                                   \
        if (condition) [[unlikely]]                                     \
        {                                                               \
            LOG_ERROR_CAT(category, "{}:{}: condition '{}' failed: {}", \
                          __FILE__, __LINE__, #condition, message);     \
            return;                                                     \
        }                                                               \
    } while (0)

#define ERR_FAIL_COND_V_MSG_CAT(category, condition, retval, message)   \
    do                                                                  \
    {                                                                   \
        if (condition) [[unlikely]]                                     \
        {                                                               \
            LOG_ERROR_CAT(category, "{}:{}: condition '{}' failed: {}", \
                          __FILE__, __LINE__, #condition, message);     \
            return retval;                                              \
        }                                                               \
    } while (0)

