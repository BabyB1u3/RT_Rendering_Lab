#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan RHI classes for early clear/present bring-up.

#include <array>
#include <limits>
#include <vector>

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#endif

#include <volk/volk.h>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class VulkanDevice;
class VulkanSwapchainTexture;
class VulkanSwapchainImageView;

class VulkanCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    VulkanCommandList() = default;
    ~VulkanCommandList();

    VulkanCommandList(const VulkanCommandList&) = delete;
    VulkanCommandList& operator=(const VulkanCommandList&) = delete;
    VulkanCommandList(VulkanCommandList&&) = delete;
    VulkanCommandList& operator=(VulkanCommandList&&) = delete;

    void initialize(VkDevice device, VkCommandPool commandPool);
    void shutdown();
    void beginRendering(const RenderingInfo& renderingInfo) override;
    void endRendering() override;

    VkCommandBuffer getVkCommandBuffer() const { return m_CommandBuffer; }
    bool isRenderingActive() const { return m_IsRendering; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

class VulkanSwapchain final : public Swapchain
{
public:
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);
    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    uint32_t acquireNextImage() override;
    Texture* getImage(uint32_t imageIndex) const override;
    TextureView* getImageView(uint32_t imageIndex) const override;
    void present(uint32_t imageIndex) override;
    void resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t width() const override { return m_Desc.width; }
    uint32_t height() const override { return m_Desc.height; }
    Format format() const override { return m_Desc.format; }
    uint32_t imageCount() const override { return static_cast<uint32_t>(m_Images.size()); }

    VkSwapchainKHR getVkSwapchain() const { return m_Swapchain; }
    VkFormat getVkFormat() const { return m_VkFormat; }

private:
    void recreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void destroySwapchain();
    TextureDesc buildSwapchainImageDesc() const;

private:
    // Now used by the real Vulkan swapchain ownership path.
    VulkanDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkFormat m_VkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<Scope<VulkanSwapchainTexture>> m_Images;
    std::vector<Scope<VulkanSwapchainImageView>> m_ImageViews;
};

class VulkanDevice final : public RHIInternal::ShellDeviceBase
{
public:
    VulkanDevice();
    ~VulkanDevice() override;

    Scope<Swapchain> createSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;

    CommandList* beginCommandList() override;
    void submit(CommandList* commandList) override;
    FrameContext* beginFrame() override;
    void endFrame(FrameContext* frameContext) override;

    VkInstance getVkInstance() const { return m_Instance; }
    VkPhysicalDevice getVkPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice getVkDevice() const { return m_Device; }
    VkSurfaceKHR getVkSurface() const { return m_Surface; }
    VkQueue getGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue getPresentQueue() const { return m_PresentQueue; }
    uint32_t getGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    uint32_t getPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkCommandPool getVkCommandPool() const { return m_CommandPool; }
    VkSemaphore getCurrentImageAvailableSemaphore() const;
    VkSemaphore getCurrentRenderFinishedSemaphore() const;
    void recycleCurrentRenderFinishedSemaphore();
    void advanceFrameSync();

private:
    struct FrameSync
    {
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
    };

    FrameSync& currentFrameSync();
    const FrameSync& currentFrameSync() const;
    void initializeInstance(NativeWindowSystem windowSystem);
    void initializeFrameSyncObjects();
    void shutdownFrameSyncObjects();
    void initializePresentationObjects(const NativeWindowHandle& nativeWindowHandle);
    void shutdownPresentationObjects();

private:
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    uint32_t m_PresentQueueFamily = std::numeric_limits<uint32_t>::max();
    NativeWindowHandle m_NativeWindowHandle{};
    bool m_HasPresentationObjects = false;
    // Frames in flight for CPU/GPU pacing; intentionally independent from swapchain imageCount.
    std::array<FrameSync, 2> m_FrameSyncObjects{};
    uint32_t m_CurrentFrameSlot = 0;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;

    VulkanCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
