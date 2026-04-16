#pragma once

/// @file MetalDevice.h
/// @brief Backend-private Metal RHI classes for early clear/present bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

struct MetalCommandListData;
struct MetalSwapchainData;
struct MetalDeviceData;
class MetalDevice;

class MetalCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    MetalCommandList();
    ~MetalCommandList() override;

    MetalCommandList(const MetalCommandList&) = delete;
    MetalCommandList& operator=(const MetalCommandList&) = delete;
    MetalCommandList(MetalCommandList&&) = delete;
    MetalCommandList& operator=(MetalCommandList&&) = delete;

    void Initialize(MetalDeviceData* deviceData);
    void Shutdown();
    void BeginRendering(const RenderingInfo& renderingInfo) override;
    void EndRendering() override;

    bool IsRenderingActive() const { return m_IsRendering; }

private:
    MetalCommandListData* m_Data = nullptr;
};

class MetalSwapchain final : public Swapchain
{
public:
    MetalSwapchain(MetalDevice& device, const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);
    ~MetalSwapchain() override;

    MetalSwapchain(const MetalSwapchain&) = delete;
    MetalSwapchain& operator=(const MetalSwapchain&) = delete;
    MetalSwapchain(MetalSwapchain&&) = delete;
    MetalSwapchain& operator=(MetalSwapchain&&) = delete;

    uint32_t AcquireNextImage() override;
    Texture* GetImage(uint32_t imageIndex) const override;
    TextureView* GetImageView(uint32_t imageIndex) const override;
    void Present(uint32_t imageIndex) override;
    void Resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t GetWidth() const override { return m_Desc.m_Width; }
    uint32_t GetHeight() const override { return m_Desc.m_Height; }
    Format GetFormat() const override { return m_Desc.m_Format; }
    uint32_t GetImageCount() const override { return 1; }

private:
    void UpdateDrawableSize();

private:
    MetalDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    MetalSwapchainData* m_Data = nullptr;
    Scope<Texture> m_Image;
    Scope<TextureView> m_ImageView;
};

class MetalDevice final : public RHIInternal::ShellDeviceBase
{
public:
    MetalDevice();
    ~MetalDevice() override;

    Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;

    CommandList* BeginCommandList() override;
    void Submit(CommandList* commandList) override;
    FrameContext* BeginFrame() override;
    void EndFrame(FrameContext* frameContext) override;

    MetalDeviceData* GetData() const { return m_Data; }
    void PresentDrawable(void* drawable);

private:
    MetalDeviceData* m_Data = nullptr;
    MetalCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
