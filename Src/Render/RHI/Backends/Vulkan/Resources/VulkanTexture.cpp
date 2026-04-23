#include "Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h"

#include <algorithm>
#include <string>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"

using namespace VulkanRHI;

namespace
{
TextureAspect ResolveDefaultTextureViewAspect(const TextureDesc& textureDesc)
{
    if (IsDepthFormat(textureDesc.m_Format))
    {
        return HasStencilComponent(textureDesc.m_Format) ? (TextureAspect::Depth | TextureAspect::Stencil)
                                                         : TextureAspect::Depth;
    }

    return TextureAspect::Color;
}

TextureViewDesc BuildDefaultTextureViewDesc(const TextureDesc& textureDesc)
{
    TextureViewDesc viewDesc;
    viewDesc.m_Type = textureDesc.m_Type;
    viewDesc.m_Format = textureDesc.m_Format;
    viewDesc.m_Aspect = ResolveDefaultTextureViewAspect(textureDesc);
    viewDesc.m_BaseMipLevel = 0;
    viewDesc.m_MipLevelCount = std::max(textureDesc.m_MipLevels, 1u);
    viewDesc.m_BaseArrayLayer = 0;
    viewDesc.m_ArrayLayerCount = std::max(textureDesc.m_ArrayLayers, 1u);
    return viewDesc;
}
} // namespace

VkImage GetVkImageFromTexture(Texture* texture)
{
    if (auto* vulkanTexture = dynamic_cast<VulkanTexture*>(texture))
        return vulkanTexture->GetVkImage();
    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
        return swapchainTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

VkImage GetVkImageFromOwnedTexture(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false,
                      "Vulkan CreateTextureView only accepts device-created textures. "
                      "Swapchain images expose views via Swapchain::GetImageView().");
    return VK_NULL_HANDLE;
}

VkImageView GetVkImageViewFromTextureView(TextureView* textureView)
{
    if (auto* vulkanTextureView = dynamic_cast<VulkanTextureView*>(textureView))
        return vulkanTextureView->GetVkImageView();
    if (auto* swapchainImageView = dynamic_cast<VulkanSwapchainImageView*>(textureView))
        return swapchainImageView->GetVkImageView();

    RTRLAB_ASSERT_MSG(false, "TextureView is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

Scope<TextureView> CreateDefaultVulkanTextureView(VkDevice device, Texture* texture)
{
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan default texture-view creation requires a valid Texture.");

    const TextureDesc& textureDesc = texture->GetDesc();
    const TextureViewDesc viewDesc = BuildDefaultTextureViewDesc(textureDesc);

    VkImageViewCreateInfo createInfo = MakeVkStruct<VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO>();
    createInfo.image = GetVkImageFromTexture(texture);
    createInfo.viewType = ToVkImageViewType(viewDesc.m_Type);
    createInfo.format = ToVkFormat(viewDesc.m_Format);
    createInfo.subresourceRange.aspectMask = ToVkImageAspect(viewDesc.m_Aspect, viewDesc.m_Format);
    createInfo.subresourceRange.baseMipLevel = viewDesc.m_BaseMipLevel;
    createInfo.subresourceRange.levelCount = viewDesc.m_MipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = viewDesc.m_BaseArrayLayer;
    createInfo.subresourceRange.layerCount = viewDesc.m_ArrayLayerCount;

    VkImageView imageView = VK_NULL_HANDLE;
    CheckVk(vkCreateImageView(device, &createInfo, nullptr, &imageView), "vkCreateImageView(default texture view)");
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetVulkanDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), debugName.c_str());

    return CreateScope<VulkanTextureView>(device, texture, imageView, viewDesc);
}
