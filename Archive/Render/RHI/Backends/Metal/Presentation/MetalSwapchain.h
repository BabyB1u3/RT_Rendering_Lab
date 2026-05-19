#pragma once

/// @file MetalSwapchain.h
/// @brief Backend-private Metal swapchain wrapper.

#include "Core/Util/Base.h"
#include "Render/RHI/NativeWindowHandle.h"
#include "Render/RHI/RHIResources.h"

struct MetalSwapchainData;
class MetalDevice;

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
    void SyncDrawableSizeToLayer();

    MetalDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    MetalSwapchainData* m_Data = nullptr;
    Scope<Texture> m_Image;
    Scope<TextureView> m_ImageView;
};
