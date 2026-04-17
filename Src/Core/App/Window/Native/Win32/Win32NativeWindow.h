#pragma once

/// @file Win32NativeWindow.h
/// @brief Windows-specific helper for extracting a NativeWindowHandle from GLFW.

#include "Render/RHI/NativeWindowHandle.h"

struct GLFWwindow;

NativeWindowHandle CreateWin32NativeWindowHandle(GLFWwindow* window);
