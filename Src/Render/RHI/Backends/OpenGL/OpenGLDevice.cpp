#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <GLFW/glfw3.h>

#include "Core/Diagnostics/Assert/Assert.h"

OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

void OpenGLSwapchain::present(uint32_t imageIndex)
{
    ShellSwapchainBase::present(imageIndex);

    // v1 relies on the current single-window/single-context invariant:
    // the GLFW current context is expected to belong to this swapchain's window.
    // Revisit this path before adding multi-window or multi-context OpenGL support.
    GLFWwindow *currentContext = glfwGetCurrentContext();
    RTRLAB_ASSERT_MSG(currentContext != nullptr,
                      "OpenGLSwapchain::present requires a current GLFW OpenGL context.");
    glfwSwapBuffers(currentContext);
}

Scope<Swapchain> OpenGLDevice::createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
{
    return CreateScope<OpenGLSwapchain>(desc, nativeWindowHandle);
}

CommandList *OpenGLDevice::beginCommandList()
{
    return &m_CommandList;
}

FrameContext *OpenGLDevice::beginFrame()
{
    return &m_FrameContext;
}
