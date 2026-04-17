#include "Core/App/Window/Native/Win32/Win32NativeWindow.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Core/Diagnostics/Assert/Assert.h"

NativeWindowHandle CreateWin32NativeWindowHandle(GLFWwindow* window)
{
    RTRLAB_ASSERT_MSG(window != nullptr, "GLFW window is null.");

    NativeWindowHandle nativeWindowHandle{};
    nativeWindowHandle.m_System = NativeWindowSystem::Win32;
    nativeWindowHandle.m_Window = reinterpret_cast<uintptr_t>(glfwGetWin32Window(window));
    return nativeWindowHandle;
}
