#pragma once

/// @file NativeWindowHandle.h
/// @brief Small platform-native window handle package consumed by the RHI.

#include <cstdint>

enum class NativeWindowSystem
{
    Win32,
    Cocoa,
    Xlib,
    Xcb,
    Wayland,
};

struct NativeWindowHandle
{
    NativeWindowSystem system = NativeWindowSystem::Win32;
    uintptr_t window = 0;
    void *display = nullptr;
    void *layer = nullptr;
};
