#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
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
