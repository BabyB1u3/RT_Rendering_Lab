#pragma once

/// @file VulkanConversions.h
/// @brief Public-RHI <-> Vulkan/VMA enum and flag converters used across the
/// Vulkan backend translation units.

#include <string>

#include "Render/RHI/Backends/Vulkan/VulkanCommon.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIPipeline.h"
#include "Render/RHI/RHIResources.h"
#include "Render/Shader/ShaderTypes.h"

class Texture;

namespace VulkanRHI
{
VkFormat ToVkFormat(Format format);
Format ToRhiFormat(VkFormat format);

bool IsDepthFormat(Format format);
bool HasStencilComponent(Format format);

VkBufferUsageFlags ToVkBufferUsage(BufferUsage usageMask);
VkImageUsageFlags ToVkImageUsage(TextureUsage usageMask);

VkImageType ToVkImageType(TextureType type);
VkImageViewType ToVkImageViewType(TextureType type);

VkFilter ToVkFilter(FilterMode mode);
VkSamplerMipmapMode ToVkMipmapMode(MipFilterMode mode);
VkSamplerAddressMode ToVkAddressMode(AddressMode mode);

VkShaderStageFlags ToVkShaderStageFlags(ShaderStage stageMask);

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology topology);
VkCullModeFlags ToVkCullMode(CullMode cullMode);
VkFrontFace ToVkFrontFace(FrontFace frontFace);
VkPolygonMode ToVkPolygonMode(FillMode fillMode);
VkCompareOp ToVkCompareOp(CompareOp compareOp);

VkBlendFactor ToVkBlendFactor(BlendFactor blendFactor);
VkBlendOp ToVkBlendOp(BlendOp blendOp);
VkColorComponentFlags ToVkColorWriteMask(uint8_t colorWriteMask);

VkDescriptorType ToVkDescriptorType(ResourceKind resourceKind);
VkImageAspectFlags ToVkImageAspect(TextureAspect aspect, Format format);

VmaMemoryUsage ToVmaMemoryUsage(MemoryUsage memoryUsage);
VmaAllocationCreateFlags ToVmaAllocationCreateFlags(MemoryUsage memoryUsage);

VkIndexType ToVkIndexType(IndexType indexType);

bool HasDebugName(const char* debugName);
void SetVulkanDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* debugName);
std::string MakeTextureViewDebugName(const Texture& texture);
} // namespace VulkanRHI
