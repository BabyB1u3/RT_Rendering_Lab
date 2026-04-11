#pragma once

/// @file Base.h
/// @brief Core type aliases and utility macros used throughout the engine.
///
/// Scope<T> wraps std::unique_ptr  - exclusive ownership, zero overhead.
/// Ref<T>   wraps std::shared_ptr  - shared ownership with reference counting.
/// Prefer Scope<T> for resources owned by a single object (e.g., Window, Renderer).
/// Use Ref<T> for resources shared across subsystems (e.g., Texture2D, Mesh, Material).

#include <cstdint>
#include <memory>
#include <utility>

#define BIT(x) (1 << (x))

/// Exclusive-ownership smart pointer (unique_ptr alias).
template <typename T>
using Scope = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Scope<T> CreateScope(Args &&...args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/// Shared-ownership smart pointer (shared_ptr alias).
template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr Ref<T> CreateRef(Args &&...args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}
