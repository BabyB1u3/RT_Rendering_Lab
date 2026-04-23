#pragma once

/// @file VulkanTexture.h
/// @brief Backend-private Vulkan texture/view wrappers for both device-owned and
/// swapchain-borrowed images, plus small dynamic-cast helpers.

#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIResources.h"

class VulkanSwapchainTexture final : public Texture
{
public:
    VulkanSwapchainTexture(VkImage image, const TextureDesc& desc) : m_Image(image), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }
    TextureState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(TextureState state) { m_CurrentState = state; }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureState m_CurrentState = TextureState::Undefined;
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

class VulkanTexture final : public Texture
{
public:
    VulkanTexture(VmaAllocator allocator, VkImage image, VmaAllocation allocation, const TextureDesc& desc)
        : m_Allocator(allocator), m_Image(image), m_Allocation(allocation), m_Desc(desc)
    {
    }

    ~VulkanTexture() override
    {
        if (m_Allocator != nullptr && m_Image != VK_NULL_HANDLE)
            vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }
    TextureState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(TextureState state) { m_CurrentState = state; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkImage m_Image = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = nullptr;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureState m_CurrentState = TextureState::Undefined;
};

class VulkanTextureView final : public TextureView
{
public:
    VulkanTextureView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanTextureView() override
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

VkImage GetVkImageFromTexture(Texture* texture);
VkImage GetVkImageFromOwnedTexture(Texture* texture);
VkImageView GetVkImageViewFromTextureView(TextureView* textureView);
Scope<TextureView> CreateDefaultVulkanTextureView(VkDevice device, Texture* texture);
