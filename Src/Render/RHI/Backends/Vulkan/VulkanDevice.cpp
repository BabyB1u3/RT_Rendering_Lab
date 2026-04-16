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
    VulkanSwapchainTexture(VkImage image, const TextureDesc& desc) : m_Image(image), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class VulkanSwapchainImageView final : public TextureView
{
public:
    VulkanSwapchainImageView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanSwapchainImageView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    VkImageView GetVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture* m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

namespace
{

void CheckVk(VkResult result, const char* what)
{
    RTRLAB_ASSERTF(result == VK_SUCCESS, "{} failed with VkResult={}", what, static_cast<int>(result));
}

VkFormat ToVkFormat(Format format)
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

Format ToRhiFormat(VkFormat format)
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

std::vector<const char*> GetRequiredInstanceExtensions(NativeWindowSystem system)
{
    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};

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

VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle& nativeWindowHandle)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    switch (nativeWindowHandle.m_System)
    {
        case NativeWindowSystem::Win32:
        {
#if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            createInfo.hinstance = GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
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
            VkWaylandSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
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

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
            "vkEnumerateDeviceExtensionProperties(count)");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
            "vkEnumerateDeviceExtensionProperties(list)");

    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    }

    return false;
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

void TransitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkPipelineStageFlags SourceStageForLayout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags SourceAccessForLayout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return 0;
    }
}
} // namespace

VulkanCommandList::~VulkanCommandList()
{
    Shutdown();
}

void VulkanCommandList::Initialize(VkDevice device, VkCommandPool commandPool)
{
    Shutdown();

    m_Device = device;
    m_CommandPool = commandPool;

    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    CheckVk(vkAllocateCommandBuffers(device, &allocateInfo, &m_CommandBuffer), "vkAllocateCommandBuffers");
}

void VulkanCommandList::Shutdown()
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

void VulkanCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(renderingInfo.m_ColorAttachments.size() == 1,
                      "Early Vulkan bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_DepthAttachment.m_View == nullptr,
                      "Early Vulkan bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan BeginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan BeginRendering currently expects a swapchain texture.");

    TransitionImageLayout(m_CommandBuffer,
                          texture->GetVkImage(),
                          texture->GetCurrentLayout(),
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          SourceStageForLayout(texture->GetCurrentLayout()),
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          SourceAccessForLayout(texture->GetCurrentLayout()),
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    texture->SetCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkClearValue clearValue = {{
        colorAttachment.m_ClearValue.m_R,
        colorAttachment.m_ClearValue.m_G,
        colorAttachment.m_ClearValue.m_B,
        colorAttachment.m_ClearValue.m_A,
    }};

    VkRenderingAttachmentInfo colorAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.imageView = imageView->GetVkImageView();
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = colorAttachment.m_LoadOp == LoadOp::Clear      ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                 : colorAttachment.m_LoadOp == LoadOp::DontCare ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                                : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachmentInfo.storeOp = colorAttachment.m_StoreOp == StoreOp::DontCare ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                                                                 : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearValue;

    VkRenderingInfo vkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    vkRenderingInfo.renderArea.offset = {renderingInfo.m_RenderArea.m_X, renderingInfo.m_RenderArea.m_Y};
    vkRenderingInfo.renderArea.extent = {renderingInfo.m_RenderArea.m_Width, renderingInfo.m_RenderArea.m_Height};
    vkRenderingInfo.layerCount = 1;
    vkRenderingInfo.colorAttachmentCount = 1;
    vkRenderingInfo.pColorAttachments = &colorAttachmentInfo;

    vkCmdBeginRendering(m_CommandBuffer, &vkRenderingInfo);
}

void VulkanCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan EndRendering requires an active rendering scope.");

    const ColorAttachmentInfo& colorAttachment = m_RenderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan EndRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan EndRendering currently expects a swapchain texture.");

    vkCmdEndRendering(m_CommandBuffer);

    TransitionImageLayout(m_CommandBuffer,
                          texture->GetVkImage(),
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          0);
    texture->SetCurrentLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    ShellCommandListBase::EndRendering();
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device,
                                 const SwapchainDesc& desc,
                                 const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device), m_Desc(RHIInternal::SanitizeSwapchainDesc(desc)), m_NativeWindowHandle(nativeWindowHandle)
{
    RTRLAB_ASSERT_MSG(RHIInternal::IsNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");

    RecreateSwapchain();
}

VulkanSwapchain::~VulkanSwapchain()
{
    DestroySwapchain();
}

uint32_t VulkanSwapchain::AcquireNextImage()
{
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                            m_Swapchain,
                                            std::numeric_limits<uint64_t>::max(),
                                            m_Device.GetCurrentImageAvailableSemaphore(),
                                            VK_NULL_HANDLE,
                                            &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                       m_Swapchain,
                                       std::numeric_limits<uint64_t>::max(),
                                       m_Device.GetCurrentImageAvailableSemaphore(),
                                       VK_NULL_HANDLE,
                                       &imageIndex);
    }

    if (result != VK_SUBOPTIMAL_KHR)
        CheckVk(result, "vkAcquireNextImageKHR");
    return imageIndex;
}

Texture* VulkanSwapchain::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
    return m_Images[imageIndex].get();
}

TextureView* VulkanSwapchain::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
    return m_ImageViews[imageIndex].get();
}

void VulkanSwapchain::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    const VkSemaphore renderFinishedSemaphore = m_Device.GetCurrentRenderFinishedSemaphore();
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        m_Device.RecycleCurrentRenderFinishedSemaphore();
        m_Device.AdvanceFrameSync();
        return;
    }

    CheckVk(result, "vkQueuePresentKHR");
    m_Device.AdvanceFrameSync();
}

void VulkanSwapchain::Resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.m_Width = std::max(newWidth, 1u);
    m_Desc.m_Height = std::max(newHeight, 1u);
    RecreateSwapchain(m_Swapchain);
}

TextureDesc VulkanSwapchain::BuildSwapchainImageDesc() const
{
    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = m_Desc.m_Format;
    textureDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::RenderTarget;
    textureDesc.m_DebugName = "VulkanSwapchainImage";
    return textureDesc;
}

void VulkanSwapchain::RecreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    const VkPhysicalDevice physicalDevice = m_Device.GetVkPhysicalDevice();
    const VkSurfaceKHR surface = m_Device.GetVkSurface();
    const VkDevice device = m_Device.GetVkDevice();

    if (oldSwapchain != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    VkSurfaceCapabilitiesKHR capabilities{};
    CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(physicalDevice, surface, m_Desc.m_Format);
    const VkPresentModeKHR presentMode = ChoosePresentMode(physicalDevice, surface, m_Desc.m_Vsync);
    const VkExtent2D extent = ChooseSwapchainExtent(capabilities, m_Desc.m_Width, m_Desc.m_Height);

    uint32_t desiredImageCount = std::max(m_Desc.m_ImageCount, capabilities.minImageCount);
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

    const uint32_t queueFamilyIndices[] = {m_Device.GetGraphicsQueueFamily(), m_Device.GetPresentQueueFamily()};

    if (m_Device.GetGraphicsQueueFamily() != m_Device.GetPresentQueueFamily())
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
    CheckVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "vkCreateSwapchainKHR");

    uint32_t swapchainImageCount = 0;
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, nullptr),
            "vkGetSwapchainImagesKHR(count)");
    RTRLAB_ASSERT_MSG(swapchainImageCount > 0, "Vulkan swapchain reported zero images.");

    std::vector<VkImage> swapchainImages(swapchainImageCount);
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, swapchainImages.data()),
            "vkGetSwapchainImagesKHR(list)");

    std::vector<Scope<VulkanSwapchainTexture>> images;
    std::vector<Scope<VulkanSwapchainImageView>> imageViews;
    images.reserve(swapchainImageCount);
    imageViews.reserve(swapchainImageCount);

    m_Desc.m_Width = extent.width;
    m_Desc.m_Height = extent.height;
    m_Desc.m_ImageCount = swapchainImageCount;
    m_Desc.m_Format = ToRhiFormat(surfaceFormat.format);
    if (m_Desc.m_Format == Format::Unknown)
        m_Desc.m_Format = Format::BGRA8_UNORM;

    const TextureDesc imageDesc = BuildSwapchainImageDesc();

    for (VkImage image : swapchainImages)
    {
        auto swapchainTexture = CreateScope<VulkanSwapchainTexture>(image, imageDesc);

        VkImageViewCreateInfo viewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewCreateInfo.image = image;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = surfaceFormat.format;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        CheckVk(vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView), "vkCreateImageView");

        TextureViewDesc viewDesc;
        viewDesc.m_Type = TextureType::Tex2D;
        viewDesc.m_Format = m_Desc.m_Format;
        viewDesc.m_Aspect = TextureAspect::Color;

        imageViews.push_back(
            CreateScope<VulkanSwapchainImageView>(device, swapchainTexture.get(), imageView, viewDesc));
        images.push_back(std::move(swapchainTexture));
    }

    DestroySwapchain();

    m_Swapchain = newSwapchain;
    m_VkFormat = surfaceFormat.format;
    m_Images = std::move(images);
    m_ImageViews = std::move(imageViews);
}

void VulkanSwapchain::DestroySwapchain()
{
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device.GetVkDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

VulkanDevice::VulkanDevice()
{
    CheckVk(volkInitialize(), "volkInitialize");
}

VulkanDevice::~VulkanDevice()
{
    ShutdownPresentationObjects();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

Scope<Swapchain> VulkanDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    InitializePresentationObjects(nativeWindowHandle);
    return CreateScope<VulkanSwapchain>(*this, desc, nativeWindowHandle);
}

CommandList* VulkanDevice::BeginCommandList()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before command recording.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan command recording must happen before queue submission.");

    VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    return &m_CommandList;
}

void VulkanDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before queue submission.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan Submit requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan Submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "VulkanDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "Vulkan Submit requires EndRendering before queue submission.");

    const VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

    const FrameSync& frameSync = GetCurrentFrameSync();
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.m_ImageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.m_RenderFinished;

    CheckVk(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, frameSync.m_InFlightFence), "vkQueueSubmit");
    m_FrameSubmitted = true;
}

FrameContext* VulkanDevice::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "VulkanDevice does not support nested frame lifetimes.");

    FrameSync& frameSync = GetCurrentFrameSync();
    CheckVk(vkWaitForFences(m_Device, 1, &frameSync.m_InFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
            "vkWaitForFences");
    CheckVk(vkResetFences(m_Device, 1, &frameSync.m_InFlightFence), "vkResetFences");

    m_FrameInProgress = true;
    m_FrameSubmitted = false;
    return &m_FrameContext;
}

void VulkanDevice::EndFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "VulkanDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan EndFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan EndFrame currently expects queue submission before the frame closes.");

    m_FrameInProgress = false;
}

VkSemaphore VulkanDevice::GetCurrentImageAvailableSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Current Vulkan frame sync is only valid during an active frame.");
    return GetCurrentFrameSync().m_ImageAvailable;
}

VkSemaphore VulkanDevice::GetCurrentRenderFinishedSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Render-finished semaphore is only valid after queue submission.");
    return GetCurrentFrameSync().m_RenderFinished;
}

void VulkanDevice::AdvanceFrameSync()
{
    // The current bring-up path only advances after a submitted frame is either presented
    // or discarded during swapchain recreation. If future acquire-failure handling starts
    // skipping frames before submission, this invariant will need to be revisited.
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "Vulkan frame sync can only advance after EndFrame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan frame sync expects queue submission before presentation advances.");

    m_CurrentFrameSlot = (m_CurrentFrameSlot + 1) % static_cast<uint32_t>(m_FrameSyncObjects.size());
    m_FrameSubmitted = false;
}

VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync()
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

const VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync() const
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

void VulkanDevice::RecycleCurrentRenderFinishedSemaphore()
{
    FrameSync& frameSync = GetCurrentFrameSync();

    // Safety relies on the caller first making the queue/device idle. The current present
    // failure path does this via RecreateSwapchain() before recycling the semaphore.
    if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
        frameSync.m_RenderFinished = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
            "vkCreateSemaphore(renderFinished recycle)");
}

void VulkanDevice::InitializeInstance(NativeWindowSystem windowSystem)
{
    const std::vector<const char*> instanceExtensions = GetRequiredInstanceExtensions(windowSystem);

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

    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_Instance), "vkCreateInstance");
    volkLoadInstance(m_Instance);
}

void VulkanDevice::InitializeFrameSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_ImageAvailable),
                "vkCreateSemaphore(imageAvailable)");
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
                "vkCreateSemaphore(renderFinished)");
        CheckVk(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &frameSync.m_InFlightFence),
                "vkCreateFence(inFlight)");
    }
}

void VulkanDevice::ShutdownFrameSyncObjects()
{
    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        if (frameSync.m_ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_ImageAvailable, nullptr);
            frameSync.m_ImageAvailable = VK_NULL_HANDLE;
        }

        if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
            frameSync.m_RenderFinished = VK_NULL_HANDLE;
        }

        if (frameSync.m_InFlightFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, frameSync.m_InFlightFence, nullptr);
            frameSync.m_InFlightFence = VK_NULL_HANDLE;
        }
    }
}

void VulkanDevice::InitializePresentationObjects(const NativeWindowHandle& nativeWindowHandle)
{
    if (m_HasPresentationObjects)
    {
        RTRLAB_ASSERT_MSG(m_NativeWindowHandle.m_System == nativeWindowHandle.m_System &&
                              m_NativeWindowHandle.m_Window == nativeWindowHandle.m_Window &&
                              m_NativeWindowHandle.m_Display == nativeWindowHandle.m_Display &&
                              m_NativeWindowHandle.m_Layer == nativeWindowHandle.m_Layer,
                          "VulkanDevice currently supports a single presentation target per device.");
        return;
    }

    const std::vector<const char*> requiredExtensions = GetRequiredInstanceExtensions(nativeWindowHandle.m_System);

    if (m_Instance == VK_NULL_HANDLE)
    {
        uint32_t extensionCount = 0;
        CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
                "vkEnumerateInstanceExtensionProperties(count)");

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
                "vkEnumerateInstanceExtensionProperties(list)");

        for (const char* requiredExtension : requiredExtensions)
        {
            const auto it = std::find_if(availableExtensions.begin(),
                                         availableExtensions.end(),
                                         [requiredExtension](const VkExtensionProperties& extension)
                                         { return std::strcmp(extension.extensionName, requiredExtension) == 0; });
            RTRLAB_ASSERTF(it != availableExtensions.end(),
                           "Required Vulkan instance extension '{}' is unavailable.",
                           requiredExtension);
        }

        InitializeInstance(nativeWindowHandle.m_System);
    }

    m_Surface = CreateSurface(m_Instance, nativeWindowHandle);

    QueueFamilySelection selection{};
    m_PhysicalDevice = PickPhysicalDevice(m_Instance, m_Surface, selection);
    m_GraphicsQueueFamily = selection.m_GraphicsFamily;
    m_PresentQueueFamily = selection.m_PresentFamily;

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

    const std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceVulkan13Features vulkan13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    // Early bring-up assumes dynamic rendering is available and lets vkCreateDevice fail if it
    // is not. Feature pre-checking should be added alongside validation/debug bring-up.
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext = &vulkan13Features;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    CheckVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);

    VkCommandPoolCreateInfo commandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    CheckVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    InitializeFrameSyncObjects();
    m_CommandList.Initialize(m_Device, m_CommandPool);
    m_NativeWindowHandle = nativeWindowHandle;
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasPresentationObjects = true;
}

void VulkanDevice::ShutdownPresentationObjects()
{
    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    m_CommandList.Shutdown();

    ShutdownFrameSyncObjects();

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
