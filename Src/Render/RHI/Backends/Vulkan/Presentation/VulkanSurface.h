#pragma once

/// @file VulkanSurface.h
/// @brief Physical-device/queue selection, swapchain format/present-mode/extent
/// selection, and platform-specific VkSurfaceKHR creation helpers.

#include <limits>
#include <vector>

#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/NativeWindowHandle.h"
#include "Render/RHI/RHIResources.h"

namespace VulkanRHI
{
struct QueueFamilySelection
{
    uint32_t m_GraphicsFamily = std::numeric_limits<uint32_t>::max();
    uint32_t m_PresentFamily = std::numeric_limits<uint32_t>::max();

    bool IsComplete() const
    {
        return m_GraphicsFamily != std::numeric_limits<uint32_t>::max() &&
               m_PresentFamily != std::numeric_limits<uint32_t>::max();
    }
};

std::vector<const char*> GetPlatformInstanceExtensionCandidates();
std::vector<const char*> GetSupportedInstanceExtensions();

VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle& nativeWindowHandle);

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice);
bool SupportsPresentOnQueueFamily(VkPhysicalDevice physicalDevice, uint32_t queueFamily, VkSurfaceKHR surface);
QueueFamilySelection FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
uint32_t FindGraphicsQueueFamily(VkPhysicalDevice physicalDevice);
VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueFamilySelection& selection);
VkPhysicalDevice PickPhysicalDevice(VkInstance instance, uint32_t& graphicsQueueFamily);

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Format preferredFormat);
VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync);
VkExtent2D
ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight);
} // namespace VulkanRHI
