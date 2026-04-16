#pragma once

/// @file CocoaNativeWindow.h
/// @brief Apple-specific helper for extracting a NativeWindowHandle from GLFW.

#include "Render/RHI/NativeWindowHandle.h"

struct GLFWwindow;

NativeWindowHandle CreateCocoaNativeWindowHandle(GLFWwindow* window);
