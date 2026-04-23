#pragma once

/// @file VulkanCommon.h
/// @brief Backend-private Vulkan platform glue: volk/VMA includes plus tiny
/// inline helpers shared by every Vulkan TU.

#include <cstddef>
#include <cstdint>

#include "Core/Diagnostics/Assert/Assert.h"

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#endif
#endif

#include <volk/volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

// Vulkan headers transitively pull in <X11/Xlib.h> on Linux when
// VK_USE_PLATFORM_XLIB_KHR is set. Xlib defines `None` and `Always` as macros,
// which collide with public RHI enum members (BufferUsage::None, CompareOp::Always,
// CullMode::None, MipFilterMode::None, ...). Undef them here so every TU that
// includes VulkanCommon.h sees clean identifiers.
#ifdef None
#undef None
#endif
#ifdef Always
#undef Always
#endif

namespace VulkanRHI
{
template <typename TVkStruct, VkStructureType SType> TVkStruct MakeVkStruct()
{
    TVkStruct value{};
    value.sType = SType;
    return value;
}

inline void CheckVk(VkResult result, const char* what)
{
    RTRLAB_ASSERTF(result == VK_SUCCESS, "{} failed with VkResult={}", what, static_cast<int>(result));
}

inline uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1)
        return value;

    return (value + alignment - 1) & ~(alignment - 1);
}

inline constexpr VkDeviceSize kFrameUploadArenaInitialCapacity = 1u << 20;
} // namespace VulkanRHI
