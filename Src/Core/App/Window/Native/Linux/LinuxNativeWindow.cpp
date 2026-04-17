#include "Core/App/Window/Native/Linux/LinuxNativeWindow.h"

#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#if defined(GLAB_GLFW_X11_NATIVE)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Core/Diagnostics/Assert/Assert.h"

NativeWindowHandle CreateLinuxNativeWindowHandle(GLFWwindow* window)
{
    RTRLAB_ASSERT_MSG(window != nullptr, "GLFW window is null.");

    NativeWindowHandle nativeWindowHandle{};
    const int platform = glfwGetPlatform();

#if defined(GLAB_GLFW_WAYLAND_NATIVE)
    if (platform == GLFW_PLATFORM_WAYLAND)
    {
        nativeWindowHandle.m_System = NativeWindowSystem::Wayland;
        nativeWindowHandle.m_Window = reinterpret_cast<uintptr_t>(glfwGetWaylandWindow(window));
        nativeWindowHandle.m_Display = glfwGetWaylandDisplay();
        return nativeWindowHandle;
    }
#endif

#if defined(GLAB_GLFW_X11_NATIVE)
    if (platform == GLFW_PLATFORM_X11)
    {
        nativeWindowHandle.m_System = NativeWindowSystem::Xlib;
        nativeWindowHandle.m_Window = static_cast<uintptr_t>(glfwGetX11Window(window));
        nativeWindowHandle.m_Display = glfwGetX11Display();
        return nativeWindowHandle;
    }
#endif

    RTRLAB_ASSERT_MSG(false, "Unsupported GLFW native platform.");
    return nativeWindowHandle;
}
