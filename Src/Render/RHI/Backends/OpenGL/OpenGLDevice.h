#pragma once

/// @file OpenGLDevice.h
/// @brief Backend-private OpenGL RHI classes for the early clear/present bring-up path.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class OpenGLCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    void BeginRendering(const RenderingInfo& renderingInfo) override;
    void EndRendering() override;

    const RenderingInfo& GetRenderingInfo() const { return m_RenderingInfo; }
    bool IsRenderingActive() const { return m_IsRendering; }
};

class OpenGLSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    OpenGLSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);

    // Compatibility-backend present stays backend-owned even though GLFW created the
    // context. This keeps Window free of a public SwapBuffers-style present API.
    void Present(uint32_t imageIndex) override;
};

class OpenGLDevice final : public RHIInternal::ShellDeviceBase
{
public:
    Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;
    Scope<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Scope<Texture> CreateTexture(const TextureDesc& desc) override;
    Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) override;
    Scope<Sampler> CreateSampler(const SamplerDesc& desc) override;

    CommandList* BeginCommandList() override;
    void Submit(CommandList* commandList) override;
    FrameContext* BeginFrame() override;

private:
    OpenGLCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
