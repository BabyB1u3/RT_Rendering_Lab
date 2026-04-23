#pragma once

/// @file VulkanDescriptors.h
/// @brief Helpers that translate PipelineLayoutDesc into Vulkan descriptor-set
/// layouts, descriptor pools, descriptor sets, and VkPipelineLayout handles.

#include <vector>

#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanPipelineLayout.h"
#include "Render/RHI/RHIPipeline.h"

namespace VulkanRHI
{
std::vector<VkDescriptorSetLayout> CreateVkDescriptorSetLayouts(VkDevice device, const PipelineLayoutDesc& desc);

VkDescriptorPool CreateVkDescriptorPoolForSet(VkDevice device, const PipelineLayoutDesc& desc, uint32_t setIndex);

VkDescriptorSet AllocateVkDescriptorSet(VkDevice device,
                                        VkDescriptorPool descriptorPool,
                                        const VulkanPipelineLayout& pipelineLayout,
                                        uint32_t setIndex);

VkPipelineLayout CreateVkPipelineLayout(VkDevice device,
                                        const PipelineLayoutDesc& desc,
                                        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
} // namespace VulkanRHI
