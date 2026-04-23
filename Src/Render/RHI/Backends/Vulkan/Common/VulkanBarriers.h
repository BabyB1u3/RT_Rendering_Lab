#pragma once

/// @file VulkanBarriers.h
/// @brief Vulkan barrier helpers: RHI state -> (layout, stage mask, access mask)
/// translators, image-layout transitions, and texture-state tracking accessors
/// that read/write the backend-private current-state cached on VulkanTexture /
/// VulkanSwapchainTexture.

#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIResources.h"
#include "Render/Shader/ShaderTypes.h"

namespace VulkanRHI
{
VkImageLayout ToVkImageLayout(TextureState state, const TextureDesc& desc);
VkPipelineStageFlags TextureStateToPipelineStageMask(TextureState state, ShaderStage shaderStages, bool isDestination);
VkAccessFlags TextureStateToAccessMask(TextureState state);
VkPipelineStageFlags BufferStateToPipelineStageMask(BufferState state, ShaderStage shaderStages);
VkAccessFlags BufferStateToAccessMask(BufferState state);
VkPipelineStageFlags ToVkPipelineStageMask(ShaderStage stageMask);

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
                           uint32_t arrayLayerCount);

VkImage GetVkImageForBarrier(Texture* texture);
VkImageLayout GetTrackedImageLayout(Texture* texture);
TextureState GetTrackedTextureState(Texture* texture);
void SetTrackedTextureState(Texture* texture, TextureState state, VkImageLayout layout);
} // namespace VulkanRHI
