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

    VkPresentInfoKHR presentInfo = MakeVkStruct<VkPresentInfoKHR, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR>();
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

    VkSwapchainCreateInfoKHR createInfo =
        MakeVkStruct<VkSwapchainCreateInfoKHR, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR>();
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
        SetVulkanDebugName(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), imageDesc.m_DebugName);

        VkImageViewCreateInfo viewCreateInfo =
            MakeVkStruct<VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO>();
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
        SetVulkanDebugName(
            device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), "VulkanSwapchainImage.View");

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

