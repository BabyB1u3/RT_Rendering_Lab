#pragma once

/// @file OpenGLDevice.h
/// @brief Backend-private OpenGL RHI skeleton classes for Milestone 1 bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class OpenGLCommandList final : public RHIInternal::ShellCommandListBase
{
};

class OpenGLSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    OpenGLSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle);
};

class OpenGLDevice final : public RHIInternal::ShellDeviceBase
{
public:
    Scope<Swapchain> createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle) override;

    CommandList *beginCommandList() override;
    FrameContext *beginFrame() override;

private:
    OpenGLCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};

