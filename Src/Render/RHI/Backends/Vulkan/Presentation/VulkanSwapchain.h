#pragma once

/// @file VulkanSwapchain.h
/// @brief Backend-private Vulkan swapchain wrapper.

#include <vector>

#include "Core/Util/Base.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/NativeWindowHandle.h"
#include "Render/RHI/RHIResources.h"

class VulkanDevice;
class VulkanSwapchainTexture;
class VulkanSwapchainImageView;

class VulkanSwapchain final : public Swapchain
{
public:
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);
    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    uint32_t AcquireNextImage() override;
    Texture* GetImage(uint32_t imageIndex) const override;
    TextureView* GetImageView(uint32_t imageIndex) const override;
    void Present(uint32_t imageIndex) override;
    void Resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t GetWidth() const override { return m_Desc.m_Width; }
    uint32_t GetHeight() const override { return m_Desc.m_Height; }
    Format GetFormat() const override { return m_Desc.m_Format; }
    uint32_t GetImageCount() const override { return static_cast<uint32_t>(m_Images.size()); }

    VkSwapchainKHR GetVkSwapchain() const { return m_Swapchain; }
    VkFormat GetVkFormat() const { return m_VkFormat; }

private:
    void RecreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void DestroySwapchain();
    TextureDesc BuildSwapchainImageDesc() const;

    VulkanDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkFormat m_VkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<Scope<VulkanSwapchainTexture>> m_Images;
    std::vector<Scope<VulkanSwapchainImageView>> m_ImageViews;
};
