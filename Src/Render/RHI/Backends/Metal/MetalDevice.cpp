#include "Render/RHI/Backends/Metal/MetalDevice.h"

MetalSwapchain::MetalSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

Scope<Swapchain> MetalDevice::createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
{
    return CreateScope<MetalSwapchain>(desc, nativeWindowHandle);
}

CommandList *MetalDevice::beginCommandList()
{
    return &m_CommandList;
}

FrameContext *MetalDevice::beginFrame()
{
    return &m_FrameContext;
}
