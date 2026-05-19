#include "Render/RHI/Backends/Vulkan/Presentation/VulkanSurface.h"

#include <algorithm>
#include <cstring>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef Always
#undef Always
#endif
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#include <wayland-client.h>
#endif
#endif

namespace VulkanRHI
{
namespace
{
constexpr const char* kPortabilitySubsetExtensionName = "VK_KHR_portability_subset";
} // namespace

std::vector<const char*> GetPlatformInstanceExtensionCandidates()
{
    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

#if defined(_WIN32)
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(__APPLE__)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    extensions.push_back("VK_KHR_xlib_surface");
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    extensions.push_back("VK_KHR_wayland_surface");
#endif
#endif

    return extensions;
}

std::vector<const char*> GetSupportedInstanceExtensions()
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
            "vkEnumerateInstanceExtensionProperties(count)");

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
            "vkEnumerateInstanceExtensionProperties(list)");

    std::vector<const char*> enabledExtensions;
    for (const char* candidate : GetPlatformInstanceExtensionCandidates())
    {
        const auto it = std::find_if(availableExtensions.begin(),
                                     availableExtensions.end(),
                                     [candidate](const VkExtensionProperties& extension)
                                     { return std::strcmp(extension.extensionName, candidate) == 0; });
        if (it != availableExtensions.end())
            enabledExtensions.push_back(candidate);
    }

    const auto hasEnabledExtension = [&enabledExtensions](const char* extensionName)
    {
        return std::find_if(enabledExtensions.begin(),
                            enabledExtensions.end(),
                            [extensionName](const char* enabledExtension)
                            { return std::strcmp(enabledExtension, extensionName) == 0; }) != enabledExtensions.end();
    };

    const bool hasSurfaceExtension = hasEnabledExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    RTRLAB_ASSERT_MSG(hasSurfaceExtension, "Vulkan instance creation requires VK_KHR_surface support.");
    return enabledExtensions;
}

VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle& nativeWindowHandle)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    switch (nativeWindowHandle.m_System)
    {
        case NativeWindowSystem::Win32:
        {
#if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkWin32SurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR>();
            createInfo.hinstance = GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Win32 Vulkan surface creation is unavailable on this platform.");
#endif
            break;
        }
        case NativeWindowSystem::Cocoa:
        {
#if defined(VK_USE_PLATFORM_METAL_EXT)
            RTRLAB_ASSERT_MSG(nativeWindowHandle.m_Layer != nullptr,
                              "Cocoa Vulkan surface creation requires a CAMetalLayer native handle.");
            VkMetalSurfaceCreateInfoEXT createInfo =
                MakeVkStruct<VkMetalSurfaceCreateInfoEXT, VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT>();
            createInfo.pLayer = nativeWindowHandle.m_Layer;
            CheckVk(vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface), "vkCreateMetalSurfaceEXT");
#else
            RTRLAB_ASSERT_MSG(false, "Cocoa Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
        case NativeWindowSystem::Xlib:
        {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
            VkXlibSurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkXlibSurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR>();
            createInfo.dpy = static_cast<Display*>(nativeWindowHandle.m_Display);
            createInfo.window = static_cast<::Window>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateXlibSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Xlib Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
        case NativeWindowSystem::Xcb:
            RTRLAB_ASSERT_MSG(false, "XCB native window handles are not currently produced by the platform layer.");
            break;
        case NativeWindowSystem::Wayland:
        {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
            VkWaylandSurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkWaylandSurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR>();
            createInfo.display = static_cast<wl_display*>(nativeWindowHandle.m_Display);
            createInfo.surface = reinterpret_cast<wl_surface*>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWaylandSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Wayland Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
    }

    return surface;
}

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
            "vkEnumerateDeviceExtensionProperties(count)");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
            "vkEnumerateDeviceExtensionProperties(list)");

    bool hasSwapchain = false;
    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            hasSwapchain = true;
    }

    return hasSwapchain;
}

std::vector<const char*> GetRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
            "vkEnumerateDeviceExtensionProperties(count)");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
            "vkEnumerateDeviceExtensionProperties(list)");

    std::vector<const char*> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    const auto hasExtension = [&extensions](const char* extensionName)
    {
        return std::find_if(extensions.begin(),
                            extensions.end(),
                            [extensionName](const VkExtensionProperties& extension)
                            { return std::strcmp(extension.extensionName, extensionName) == 0; }) != extensions.end();
    };

    if (hasExtension(kPortabilitySubsetExtensionName))
        requiredExtensions.push_back(kPortabilitySubsetExtensionName);

    return requiredExtensions;
}

bool SupportsPresentOnQueueFamily(VkPhysicalDevice physicalDevice, uint32_t queueFamily, VkSurfaceKHR surface)
{
    VkBool32 presentSupported = VK_FALSE;
    CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presentSupported),
            "vkGetPhysicalDeviceSurfaceSupportKHR");
    return presentSupported == VK_TRUE;
}

QueueFamilySelection FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    QueueFamilySelection selection{};

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        const VkQueueFamilyProperties& properties = familyProperties[familyIndex];
        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            selection.m_GraphicsFamily = familyIndex;

        VkBool32 presentSupported = VK_FALSE;
        CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupported),
                "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (presentSupported == VK_TRUE)
            selection.m_PresentFamily = familyIndex;

        if (selection.IsComplete())
            break;
    }

    return selection;
}

uint32_t FindGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        if ((familyProperties[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            return familyIndex;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan graphics queue family.");
    return std::numeric_limits<uint32_t>::max();
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueFamilySelection& selection)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        QueueFamilySelection candidateSelection = FindQueueFamilies(device, surface);
        if (!candidateSelection.IsComplete())
            continue;

        selection = candidateSelection;
        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics, present, and swapchain support.");
    return VK_NULL_HANDLE;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, uint32_t& graphicsQueueFamily)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        graphicsQueueFamily = FindGraphicsQueueFamily(device);
        if (graphicsQueueFamily == std::numeric_limits<uint32_t>::max())
            continue;

        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics and swapchain support.");
    return VK_NULL_HANDLE;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Format preferredFormat)
{
    uint32_t formatCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    RTRLAB_ASSERT_MSG(formatCount > 0, "Vulkan surface reports no supported formats.");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

    const VkFormat preferredVkFormat = ToVkFormat(preferredFormat);
    for (const VkSurfaceFormatKHR& surfaceFormat : formats)
    {
        if (surfaceFormat.format == preferredVkFormat)
            return surfaceFormat;
    }

    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync)
{
    uint32_t modeCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    RTRLAB_ASSERT_MSG(modeCount > 0, "Vulkan surface reports no present modes.");

    std::vector<VkPresentModeKHR> presentModes(modeCount);
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, presentModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");

    if (vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    }

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return presentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent{};
    extent.width = std::clamp(requestedWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(requestedHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}
} // namespace VulkanRHI
