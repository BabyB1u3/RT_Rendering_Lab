#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan RHI classes for early clear/present bring-up.

#include <limits>

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

#include <Volk/volk.h>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class VulkanDevice;

class VulkanCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    ~VulkanCommandList();

    void initialize(VkDevice device, VkCommandPool commandPool);
    void shutdown();

    VkCommandBuffer getVkCommandBuffer() const { return m_CommandBuffer; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

class VulkanSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    VulkanSwapchain(VulkanDevice &device, const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle);

private:
    VulkanDevice &m_Device;
};

class VulkanDevice final : public RHIInternal::ShellDeviceBase
{
public:
    VulkanDevice();
    ~VulkanDevice() override;

    Scope<Swapchain> createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle) override;

    CommandList *beginCommandList() override;
    FrameContext *beginFrame() override;

    VkInstance getVkInstance() const { return m_Instance; }
    VkPhysicalDevice getVkPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice getVkDevice() const { return m_Device; }
    VkSurfaceKHR getVkSurface() const { return m_Surface; }
    VkQueue getGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue getPresentQueue() const { return m_PresentQueue; }
    uint32_t getGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    uint32_t getPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkCommandPool getVkCommandPool() const { return m_CommandPool; }

private:
    void initializeInstance(NativeWindowSystem windowSystem);
    void initializePresentationObjects(const NativeWindowHandle &nativeWindowHandle);
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

    VulkanCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
