#pragma once

/// @file LinuxNativeWindow.h
/// @brief Linux-specific helper for extracting a NativeWindowHandle from GLFW.

#include "Render/RHI/NativeWindowHandle.h"

struct GLFWwindow;

NativeWindowHandle CreateLinuxNativeWindowHandle(GLFWwindow *window);
