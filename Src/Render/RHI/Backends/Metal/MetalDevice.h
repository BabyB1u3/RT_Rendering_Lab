#pragma once

/// @file MetalDevice.h
/// @brief Backend-private Metal RHI skeleton classes for Milestone 1 bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class MetalCommandList final : public RHIInternal::ShellCommandListBase
{
};

class MetalSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    MetalSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle);
};

class MetalDevice final : public RHIInternal::ShellDeviceBase
{
public:
    Scope<Swapchain> createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle) override;

    CommandList *beginCommandList() override;
    FrameContext *beginFrame() override;

private:
    MetalCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};

