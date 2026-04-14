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
    // Platform-native pointers/handles in this struct are borrowed. The platform/window layer
    // keeps them alive; any RHI backend that wants to store them across calls must retain/own
    // the platform object explicitly on the backend side.
    void *display = nullptr;
    void *layer = nullptr;
};
