#include "Render/RHI/Backends/Vulkan/VulkanDevice.h"

VulkanSwapchain::VulkanSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

Scope<Swapchain> VulkanDevice::createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
{
    return CreateScope<VulkanSwapchain>(desc, nativeWindowHandle);
}

CommandList *VulkanDevice::beginCommandList()
{
    return &m_CommandList;
}

FrameContext *VulkanDevice::beginFrame()
{
    return &m_FrameContext;
}
