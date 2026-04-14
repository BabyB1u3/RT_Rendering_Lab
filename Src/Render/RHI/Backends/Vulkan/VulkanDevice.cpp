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

class VulkanSwapchainTexture final : public Texture
{
public:
    VulkanSwapchainTexture(VkImage image, const TextureDesc &desc)
        : m_Image(image), m_Desc(desc)
    {
    }

    const TextureDesc &getDesc() const override { return m_Desc; }
    VkImage getVkImage() const { return m_Image; }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    TextureDesc m_Desc;
};

class VulkanSwapchainImageView final : public TextureView
{
public:
    VulkanSwapchainImageView(VkDevice device, Texture *texture, VkImageView imageView, const TextureViewDesc &desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanSwapchainImageView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture *getTexture() const override { return m_Texture; }
    const TextureViewDesc &getDesc() const override { return m_Desc; }
    VkImageView getVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture *m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

namespace
{

    void checkVk(VkResult result, const char *what)
    {
        RTRLAB_ASSERTF(result == VK_SUCCESS, "{} failed with VkResult={}", what, static_cast<int>(result));
    }

    VkFormat toVkFormat(Format format)
    {
        switch (format)
        {
        case Format::BGRA8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::RGBA8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        default:
            return VK_FORMAT_B8G8R8A8_UNORM;
        }
    }

    Format toRhiFormat(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_B8G8R8A8_UNORM:
            return Format::BGRA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return Format::BGRA8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return Format::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return Format::RGBA8_SRGB;
        default:
            return Format::Unknown;
        }
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

    VkSurfaceFormatKHR chooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Format preferredFormat)
    {
        uint32_t formatCount = 0;
        checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
                "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
        RTRLAB_ASSERT_MSG(formatCount > 0, "Vulkan surface reports no supported formats.");

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
                "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

        const VkFormat preferredVkFormat = toVkFormat(preferredFormat);
        for (const VkSurfaceFormatKHR &surfaceFormat : formats)
        {
            if (surfaceFormat.format == preferredVkFormat)
                return surfaceFormat;
        }

        return formats.front();
    }

    VkPresentModeKHR choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync)
    {
        uint32_t modeCount = 0;
        checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr),
                "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
        RTRLAB_ASSERT_MSG(modeCount > 0, "Vulkan surface reports no present modes.");

        std::vector<VkPresentModeKHR> presentModes(modeCount);
        checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, presentModes.data()),
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

    VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t requestedWidth, uint32_t requestedHeight)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        VkExtent2D extent{};
        extent.width = std::clamp(requestedWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(requestedHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return extent;
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
    : m_Device(device),
      m_Desc(RHIInternal::sanitizeSwapchainDesc(desc)),
      m_NativeWindowHandle(nativeWindowHandle)
{
    RTRLAB_ASSERT_MSG(RHIInternal::isNativeWindowHandleValid(nativeWindowHandle), "Native window handle is incomplete.");

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    checkVk(vkCreateFence(m_Device.getVkDevice(), &fenceCreateInfo, nullptr, &m_AcquireFence), "vkCreateFence");

    recreateSwapchain();
}

VulkanSwapchain::~VulkanSwapchain()
{
    destroySwapchain();

    if (m_AcquireFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_Device.getVkDevice(), m_AcquireFence, nullptr);
        m_AcquireFence = VK_NULL_HANDLE;
    }
}

uint32_t VulkanSwapchain::acquireNextImage()
{
    checkVk(vkResetFences(m_Device.getVkDevice(), 1, &m_AcquireFence), "vkResetFences");

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        m_Device.getVkDevice(),
        m_Swapchain,
        std::numeric_limits<uint64_t>::max(),
        VK_NULL_HANDLE,
        m_AcquireFence,
        &imageIndex);

    // Step B treats SUBOPTIMAL the same as OUT_OF_DATE so resize/recreate paths are conservative
    // until Step C introduces proper queue sync and presentation semaphores.
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreateSwapchain(m_Swapchain);
        result = vkAcquireNextImageKHR(
            m_Device.getVkDevice(),
            m_Swapchain,
            std::numeric_limits<uint64_t>::max(),
            VK_NULL_HANDLE,
            m_AcquireFence,
            &imageIndex);
    }

    checkVk(result, "vkAcquireNextImageKHR");
    checkVk(vkWaitForFences(m_Device.getVkDevice(), 1, &m_AcquireFence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
            "vkWaitForFences");

    return imageIndex;
}

Texture *VulkanSwapchain::getImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
    return m_Images[imageIndex].get();
}

TextureView *VulkanSwapchain::getImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
    return m_ImageViews[imageIndex].get();
}

void VulkanSwapchain::present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    // Placeholder: Step C will provide renderFinished in pWaitSemaphores after real queue submission.
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_Device.getPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreateSwapchain(m_Swapchain);
        return;
    }

    checkVk(result, "vkQueuePresentKHR");
}

void VulkanSwapchain::resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.width = std::max(newWidth, 1u);
    m_Desc.height = std::max(newHeight, 1u);
    recreateSwapchain(m_Swapchain);
}

TextureDesc VulkanSwapchain::buildSwapchainImageDesc() const
{
    TextureDesc textureDesc;
    textureDesc.type = TextureType::Tex2D;
    textureDesc.format = m_Desc.format;
    textureDesc.extent = Extent3D{m_Desc.width, m_Desc.height, 1};
    textureDesc.mipLevels = 1;
    textureDesc.arrayLayers = 1;
    textureDesc.usageMask = TextureUsage::RenderTarget;
    textureDesc.debugName = "VulkanSwapchainImage";
    return textureDesc;
}

void VulkanSwapchain::recreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    const VkPhysicalDevice physicalDevice = m_Device.getVkPhysicalDevice();
    const VkSurfaceKHR surface = m_Device.getVkSurface();
    const VkDevice device = m_Device.getVkDevice();

    if (oldSwapchain != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    VkSurfaceCapabilitiesKHR capabilities{};
    checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(physicalDevice, surface, m_Desc.format);
    const VkPresentModeKHR presentMode = choosePresentMode(physicalDevice, surface, m_Desc.vsync);
    const VkExtent2D extent = chooseSwapchainExtent(capabilities, m_Desc.width, m_Desc.height);

    uint32_t desiredImageCount = std::max(m_Desc.imageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
        desiredImageCount = std::min(desiredImageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = desiredImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t queueFamilyIndices[] = {
        m_Device.getGraphicsQueueFamily(),
        m_Device.getPresentQueueFamily()
    };

    if (m_Device.getGraphicsQueueFamily() != m_Device.getPresentQueueFamily())
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    checkVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "vkCreateSwapchainKHR");

    uint32_t swapchainImageCount = 0;
    checkVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
    RTRLAB_ASSERT_MSG(swapchainImageCount > 0, "Vulkan swapchain reported zero images.");

    std::vector<VkImage> swapchainImages(swapchainImageCount);
    checkVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

    std::vector<Scope<VulkanSwapchainTexture>> images;
    std::vector<Scope<VulkanSwapchainImageView>> imageViews;
    images.reserve(swapchainImageCount);
    imageViews.reserve(swapchainImageCount);

    m_Desc.width = extent.width;
    m_Desc.height = extent.height;
    m_Desc.imageCount = swapchainImageCount;
    m_Desc.format = toRhiFormat(surfaceFormat.format);
    if (m_Desc.format == Format::Unknown)
        m_Desc.format = Format::BGRA8_UNORM;

    const TextureDesc imageDesc = buildSwapchainImageDesc();

    for (VkImage image : swapchainImages)
    {
        auto swapchainTexture = CreateScope<VulkanSwapchainTexture>(image, imageDesc);

        VkImageViewCreateInfo viewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewCreateInfo.image = image;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = surfaceFormat.format;
        viewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        checkVk(vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView), "vkCreateImageView");

        TextureViewDesc viewDesc;
        viewDesc.type = TextureType::Tex2D;
        viewDesc.format = m_Desc.format;
        viewDesc.aspect = TextureAspect::Color;

        imageViews.push_back(CreateScope<VulkanSwapchainImageView>(device, swapchainTexture.get(), imageView, viewDesc));
        images.push_back(std::move(swapchainTexture));
    }

    destroySwapchain();

    m_Swapchain = newSwapchain;
    m_VkFormat = surfaceFormat.format;
    m_Images = std::move(images);
    m_ImageViews = std::move(imageViews);

}

void VulkanSwapchain::destroySwapchain()
{
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device.getVkDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
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
    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

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
