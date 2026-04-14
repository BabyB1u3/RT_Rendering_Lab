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

    MetalCommandList(const MetalCommandList &) = delete;
    MetalCommandList &operator=(const MetalCommandList &) = delete;
    MetalCommandList(MetalCommandList &&) = delete;
    MetalCommandList &operator=(MetalCommandList &&) = delete;

    void initialize(MetalDeviceData *deviceData);
    void shutdown();
    void beginRendering(const RenderingInfo &renderingInfo) override;
    void endRendering() override;

    bool isRenderingActive() const { return m_IsRendering; }

private:
    MetalCommandListData *m_Data = nullptr;
};

class MetalSwapchain final : public Swapchain
{
public:
    MetalSwapchain(MetalDevice &device, const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle);
    ~MetalSwapchain() override;

    MetalSwapchain(const MetalSwapchain &) = delete;
    MetalSwapchain &operator=(const MetalSwapchain &) = delete;
    MetalSwapchain(MetalSwapchain &&) = delete;
    MetalSwapchain &operator=(MetalSwapchain &&) = delete;

    uint32_t acquireNextImage() override;
    Texture *getImage(uint32_t imageIndex) const override;
    TextureView *getImageView(uint32_t imageIndex) const override;
    void present(uint32_t imageIndex) override;
    void resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t width() const override { return m_Desc.width; }
    uint32_t height() const override { return m_Desc.height; }
    Format format() const override { return m_Desc.format; }
    uint32_t imageCount() const override { return 1; }

private:
    void updateDrawableSize();

private:
    MetalDevice &m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    MetalSwapchainData *m_Data = nullptr;
    Scope<Texture> m_Image;
    Scope<TextureView> m_ImageView;
};

class MetalDevice final : public RHIInternal::ShellDeviceBase
{
public:
    MetalDevice();
    ~MetalDevice() override;

    Scope<Swapchain> createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle) override;

    CommandList *beginCommandList() override;
    void submit(CommandList *commandList) override;
    FrameContext *beginFrame() override;
    void endFrame(FrameContext *frameContext) override;

    MetalDeviceData *getData() const { return m_Data; }
    void presentDrawable(void *drawable);

private:
    MetalDeviceData *m_Data = nullptr;
    MetalCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};

