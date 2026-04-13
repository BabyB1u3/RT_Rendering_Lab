#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan RHI skeleton classes for Milestone 1 bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class VulkanCommandList final : public RHIInternal::ShellCommandListBase
{
};

class VulkanSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    VulkanSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle);
};

class VulkanDevice final : public RHIInternal::ShellDeviceBase
{
public:
    Scope<Swapchain> createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle) override;

    CommandList *beginCommandList() override;
    FrameContext *beginFrame() override;

private:
    VulkanCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};

