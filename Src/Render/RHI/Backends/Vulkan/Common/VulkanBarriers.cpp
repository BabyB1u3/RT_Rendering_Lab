#include "Render/RHI/Backends/Vulkan/Common/VulkanBarriers.h"

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h"

namespace VulkanRHI
{
VkImageLayout ToVkImageLayout(TextureState state, const TextureDesc& desc)
{
    switch (state)
    {
        case TextureState::Undefined:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case TextureState::RenderTarget:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case TextureState::DepthStencil:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case TextureState::ShaderRead:
            return IsDepthFormat(desc.m_Format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case TextureState::ShaderReadWrite:
            return VK_IMAGE_LAYOUT_GENERAL;
        case TextureState::CopySource:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case TextureState::CopyDest:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case TextureState::Present:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkPipelineStageFlags ToVkPipelineStageMask(ShaderStage stageMask)
{
    VkPipelineStageFlags result = 0;
    if ((stageMask & ShaderStage::Vertex) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if ((stageMask & ShaderStage::Fragment) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if ((stageMask & ShaderStage::Compute) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    return result;
}

VkPipelineStageFlags TextureStateToPipelineStageMask(TextureState state, ShaderStage shaderStages, bool isDestination)
{
    switch (state)
    {
        case TextureState::RenderTarget:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case TextureState::DepthStencil:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case TextureState::ShaderRead:
        case TextureState::ShaderReadWrite:
        {
            const VkPipelineStageFlags shaderStageMask =
                ToVkPipelineStageMask(shaderStages == ShaderStage::None ? ShaderStage::All : shaderStages);
            return shaderStageMask != 0 ? shaderStageMask : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        case TextureState::CopySource:
        case TextureState::CopyDest:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case TextureState::Present:
            return isDestination ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case TextureState::Undefined:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags TextureStateToAccessMask(TextureState state)
{
    switch (state)
    {
        case TextureState::RenderTarget:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case TextureState::DepthStencil:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case TextureState::ShaderRead:
            return VK_ACCESS_SHADER_READ_BIT;
        case TextureState::ShaderReadWrite:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case TextureState::CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case TextureState::CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case TextureState::Present:
        case TextureState::Undefined:
        default:
            return 0;
    }
}

VkPipelineStageFlags BufferStateToPipelineStageMask(BufferState state, ShaderStage shaderStages)
{
    switch (state)
    {
        case BufferState::VertexIndex:
            return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case BufferState::UniformRead:
        case BufferState::StorageRead:
        case BufferState::StorageReadWrite:
        {
            const VkPipelineStageFlags shaderStageMask =
                ToVkPipelineStageMask(shaderStages == ShaderStage::None ? ShaderStage::All : shaderStages);
            return shaderStageMask != 0 ? shaderStageMask : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        case BufferState::CopySource:
        case BufferState::CopyDest:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case BufferState::IndirectArgument:
            return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        case BufferState::Undefined:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags BufferStateToAccessMask(BufferState state)
{
    switch (state)
    {
        case BufferState::VertexIndex:
            return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
        case BufferState::UniformRead:
            return VK_ACCESS_UNIFORM_READ_BIT;
        case BufferState::StorageRead:
            return VK_ACCESS_SHADER_READ_BIT;
        case BufferState::StorageReadWrite:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case BufferState::CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case BufferState::CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case BufferState::IndirectArgument:
            return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        case BufferState::Undefined:
        default:
            return 0;
    }
}

void TransitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkImageAspectFlags aspectMask,
                           uint32_t mipLevelCount,
                           uint32_t arrayLayerCount)
{
    VkImageMemoryBarrier barrier = MakeVkStruct<VkImageMemoryBarrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER>();
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayerCount;

    vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkImage GetVkImageForBarrier(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetVkImage();
    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
        return swapchainTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

VkImageLayout GetTrackedImageLayout(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetCurrentLayout();
    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
        return swapchainTexture->GetCurrentLayout();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

TextureState GetTrackedTextureState(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetCurrentState();
    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
        return swapchainTexture->GetCurrentState();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return TextureState::Undefined;
}

void SetTrackedTextureState(Texture* texture, TextureState state, VkImageLayout layout)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
    {
        ownedTexture->SetCurrentState(state);
        ownedTexture->SetCurrentLayout(layout);
        return;
    }

    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
    {
        swapchainTexture->SetCurrentState(state);
        swapchainTexture->SetCurrentLayout(layout);
        return;
    }

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
}
} // namespace VulkanRHI
