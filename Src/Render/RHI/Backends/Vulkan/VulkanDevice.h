#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan RHI classes for early clear/present bring-up.

#include <array>
#include <limits>
#include <vector>

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
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

    void Initialize(VkDevice device, VkCommandPool commandPool);
    void Shutdown();
    void BeginRendering(const RenderingInfo& renderingInfo) override;
    void EndRendering() override;

    VkCommandBuffer GetVkCommandBuffer() const { return m_CommandBuffer; }
    bool IsRenderingActive() const { return m_IsRendering; }

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

    Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;
    Scope<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Scope<Texture> CreateTexture(const TextureDesc& desc) override;
    Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) override;
    Scope<Sampler> CreateSampler(const SamplerDesc& desc) override;

    CommandList* BeginCommandList() override;
    void Submit(CommandList* commandList) override;
    FrameContext* BeginFrame() override;
    void EndFrame(FrameContext* frameContext) override;

    VkInstance GetVkInstance() const { return m_Instance; }
    VkPhysicalDevice GetVkPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetVkDevice() const { return m_Device; }
    VkSurfaceKHR GetVkSurface() const { return m_Surface; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    uint32_t GetPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkCommandPool GetVkCommandPool() const { return m_CommandPool; }
    VkSemaphore GetCurrentImageAvailableSemaphore() const;
    VkSemaphore GetCurrentRenderFinishedSemaphore() const;
    void RecycleCurrentRenderFinishedSemaphore();
    void AdvanceFrameSync();

private:
    struct FrameSync
    {
        VkSemaphore m_ImageAvailable = VK_NULL_HANDLE;
        VkSemaphore m_RenderFinished = VK_NULL_HANDLE;
        VkFence m_InFlightFence = VK_NULL_HANDLE;
    };

    FrameSync& GetCurrentFrameSync();
    const FrameSync& GetCurrentFrameSync() const;
    void InitializeInstance();
    void InitializeDeviceObjects();
    void InitializeDeviceObjectsForSurface(VkSurfaceKHR surface);
    void InitializeFrameSyncObjects();
    void ShutdownDeviceObjects();
    void ShutdownFrameSyncObjects();
    void InitializePresentationObjects(const NativeWindowHandle& nativeWindowHandle);
    void ShutdownPresentationObjects();

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
    bool m_HasDeviceObjects = false;
    bool m_HasPresentationObjects = false;
    // Frames in flight for CPU/GPU pacing; intentionally independent from swapchain imageCount.
    std::array<FrameSync, 2> m_FrameSyncObjects{};
    uint32_t m_CurrentFrameSlot = 0;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;

    VulkanCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
