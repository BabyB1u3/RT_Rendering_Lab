#include "Render/RHI/Backends/Vulkan/VulkanDevice.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#include <X11/Xlib.h>
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#include <wayland-client.h>
#endif
#endif

namespace
{
    void checkVk(VkResult result, const char *what)
    {
        RTRLAB_ASSERTF(result == VK_SUCCESS, "{} failed with VkResult={}", what, static_cast<int>(result));
    }

    std::vector<const char *> getRequiredInstanceExtensions(NativeWindowSystem system)
    {
        std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};

        switch (system)
        {
        case NativeWindowSystem::Win32:
            extensions.push_back("VK_KHR_win32_surface");
            break;
        case NativeWindowSystem::Cocoa:
            extensions.push_back("VK_EXT_metal_surface");
            break;
        case NativeWindowSystem::Xlib:
            extensions.push_back("VK_KHR_xlib_surface");
            break;
        case NativeWindowSystem::Xcb:
            extensions.push_back("VK_KHR_xcb_surface");
            break;
        case NativeWindowSystem::Wayland:
            extensions.push_back("VK_KHR_wayland_surface");
            break;
        }

        return extensions;
    }

    VkSurfaceKHR createSurface(VkInstance instance, const NativeWindowHandle &nativeWindowHandle)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        switch (nativeWindowHandle.system)
        {
        case NativeWindowSystem::Win32:
        {
#if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            createInfo.hinstance = GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(nativeWindowHandle.window);
            checkVk(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Win32 Vulkan surface creation is unavailable on this platform.");
#endif
            break;
        }
        case NativeWindowSystem::Cocoa:
            RTRLAB_ASSERT_MSG(false, "Cocoa Vulkan surface creation is not compiled in this backend build.");
            break;
        case NativeWindowSystem::Xlib:
        {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
            VkXlibSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
            createInfo.dpy = static_cast<Display *>(nativeWindowHandle.display);
            createInfo.window = static_cast<::Window>(nativeWindowHandle.window);
            checkVk(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateXlibSurfaceKHR");
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
            VkWaylandSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
            createInfo.display = static_cast<wl_display *>(nativeWindowHandle.display);
            createInfo.surface = reinterpret_cast<wl_surface *>(nativeWindowHandle.window);
            checkVk(vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWaylandSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Wayland Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
        }

        return surface;
    }

    struct QueueFamilySelection
    {
        uint32_t graphicsFamily = std::numeric_limits<uint32_t>::max();
        uint32_t presentFamily = std::numeric_limits<uint32_t>::max();

        bool isComplete() const
        {
            return graphicsFamily != std::numeric_limits<uint32_t>::max() &&
                   presentFamily != std::numeric_limits<uint32_t>::max();
        }
    };

    QueueFamilySelection findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
    {
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

        std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

        QueueFamilySelection selection{};

        for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
        {
            const VkQueueFamilyProperties &properties = familyProperties[familyIndex];
            if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                selection.graphicsFamily = familyIndex;

            VkBool32 presentSupported = VK_FALSE;
            checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupported),
                    "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (presentSupported == VK_TRUE)
                selection.presentFamily = familyIndex;

            if (selection.isComplete())
                break;
        }

        return selection;
    }

    bool supportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
    {
        uint32_t extensionCount = 0;
        checkVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
                "vkEnumerateDeviceExtensionProperties(count)");

        std::vector<VkExtensionProperties> extensions(extensionCount);
        checkVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
                "vkEnumerateDeviceExtensionProperties(list)");

        for (const VkExtensionProperties &extension : extensions)
        {
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                return true;
        }

        return false;
    }

    VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueFamilySelection &selection)
    {
        uint32_t deviceCount = 0;
        checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
        RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

        for (VkPhysicalDevice device : devices)
        {
            if (!supportsRequiredDeviceExtensions(device))
                continue;

            QueueFamilySelection candidateSelection = findQueueFamilies(device, surface);
            if (!candidateSelection.isComplete())
                continue;

            selection = candidateSelection;
            return device;
        }

        RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics, present, and swapchain support.");
        return VK_NULL_HANDLE;
    }
}

VulkanCommandList::~VulkanCommandList()
{
    shutdown();
}

void VulkanCommandList::initialize(VkDevice device, VkCommandPool commandPool)
{
    shutdown();

    m_Device = device;
    m_CommandPool = commandPool;

    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    checkVk(vkAllocateCommandBuffers(device, &allocateInfo, &m_CommandBuffer), "vkAllocateCommandBuffers");
}

void VulkanCommandList::shutdown()
{
    // Idempotent: VulkanDevice tears the command list down explicitly before member destruction.
    if (m_Device != VK_NULL_HANDLE && m_CommandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
    }

    m_CommandBuffer = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_Device = VK_NULL_HANDLE;
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice &device, const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle), m_Device(device)
{
}

VulkanDevice::VulkanDevice()
{
    checkVk(volkInitialize(), "volkInitialize");
}

VulkanDevice::~VulkanDevice()
{
    shutdownPresentationObjects();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

Scope<Swapchain> VulkanDevice::createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
{
    initializePresentationObjects(nativeWindowHandle);
    return CreateScope<VulkanSwapchain>(*this, desc, nativeWindowHandle);
}

CommandList *VulkanDevice::beginCommandList()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects, "Vulkan presentation objects must be initialized before command recording.");
    return &m_CommandList;
}

FrameContext *VulkanDevice::beginFrame()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects, "Vulkan presentation objects must be initialized before beginning a frame.");
    return &m_FrameContext;
}

void VulkanDevice::initializeInstance(NativeWindowSystem windowSystem)
{
    const std::vector<const char *> instanceExtensions = getRequiredInstanceExtensions(windowSystem);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "RTRLab";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "RTRLab";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    checkVk(vkCreateInstance(&createInfo, nullptr, &m_Instance), "vkCreateInstance");
    volkLoadInstance(m_Instance);
}

void VulkanDevice::initializePresentationObjects(const NativeWindowHandle &nativeWindowHandle)
{
    if (m_HasPresentationObjects)
    {
        RTRLAB_ASSERT_MSG(m_NativeWindowHandle.system == nativeWindowHandle.system &&
                              m_NativeWindowHandle.window == nativeWindowHandle.window &&
                              m_NativeWindowHandle.display == nativeWindowHandle.display &&
                              m_NativeWindowHandle.layer == nativeWindowHandle.layer,
                          "VulkanDevice currently supports a single presentation target per device.");
        return;
    }

    const std::vector<const char *> requiredExtensions = getRequiredInstanceExtensions(nativeWindowHandle.system);

    if (m_Instance == VK_NULL_HANDLE)
    {
        uint32_t extensionCount = 0;
        checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
                "vkEnumerateInstanceExtensionProperties(count)");

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
                "vkEnumerateInstanceExtensionProperties(list)");

        for (const char *requiredExtension : requiredExtensions)
        {
            const auto it = std::find_if(
                availableExtensions.begin(),
                availableExtensions.end(),
                [requiredExtension](const VkExtensionProperties &extension)
                { return std::strcmp(extension.extensionName, requiredExtension) == 0; });
            RTRLAB_ASSERTF(it != availableExtensions.end(), "Required Vulkan instance extension '{}' is unavailable.", requiredExtension);
        }

        initializeInstance(nativeWindowHandle.system);
    }

    m_Surface = createSurface(m_Instance, nativeWindowHandle);

    QueueFamilySelection selection{};
    m_PhysicalDevice = pickPhysicalDevice(m_Instance, m_Surface, selection);
    m_GraphicsQueueFamily = selection.graphicsFamily;
    m_PresentQueueFamily = selection.presentFamily;

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo graphicsQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(graphicsQueueCreateInfo);

    if (m_PresentQueueFamily != m_GraphicsQueueFamily)
    {
        VkDeviceQueueCreateInfo presentQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        presentQueueCreateInfo.queueFamilyIndex = m_PresentQueueFamily;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(presentQueueCreateInfo);
    }

    const std::array<const char *, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    checkVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);

    VkCommandPoolCreateInfo commandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    checkVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    m_CommandList.initialize(m_Device, m_CommandPool);
    m_NativeWindowHandle = nativeWindowHandle;
    m_HasPresentationObjects = true;
}

void VulkanDevice::shutdownPresentationObjects()
{
    m_CommandList.shutdown();

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    m_PhysicalDevice = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_PresentQueue = VK_NULL_HANDLE;
    m_GraphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    m_PresentQueueFamily = std::numeric_limits<uint32_t>::max();
    m_NativeWindowHandle = {};
    m_HasPresentationObjects = false;
}
