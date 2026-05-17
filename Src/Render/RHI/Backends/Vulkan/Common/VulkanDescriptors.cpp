#include "Render/RHI/Backends/Vulkan/Common/VulkanDescriptors.h"

#include <algorithm>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"

namespace VulkanRHI
{
std::vector<VkDescriptorSetLayout> CreateVkDescriptorSetLayouts(VkDevice device, const PipelineLayoutDesc& desc)
{
    uint32_t maxSetIndex = 0;
    bool hasBindings = false;
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        maxSetIndex = std::max(maxSetIndex, binding.m_SetIndex);
        hasBindings = true;
    }

    if (!hasBindings)
        return {};

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindingsPerSet(maxSetIndex + 1);
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.m_Binding;
        vkBinding.descriptorType = ToVkDescriptorType(binding.m_Kind);
        vkBinding.descriptorCount = binding.m_ArrayCount;
        vkBinding.stageFlags = ToVkShaderStageFlags(binding.m_StageMask);
        bindingsPerSet[binding.m_SetIndex].push_back(vkBinding);
    }

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(bindingsPerSet.size(), VK_NULL_HANDLE);
    for (size_t setIndex = 0; setIndex < bindingsPerSet.size(); ++setIndex)
    {
        VkDescriptorSetLayoutCreateInfo createInfo =
            MakeVkStruct<VkDescriptorSetLayoutCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO>();
        createInfo.bindingCount = static_cast<uint32_t>(bindingsPerSet[setIndex].size());
        createInfo.pBindings = bindingsPerSet[setIndex].empty() ? nullptr : bindingsPerSet[setIndex].data();
        CheckVk(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayouts[setIndex]),
                "vkCreateDescriptorSetLayout");
    }

    return descriptorSetLayouts;
}

VkDescriptorPool CreateVkDescriptorPoolForSet(VkDevice device, const PipelineLayoutDesc& desc, uint32_t setIndex)
{
    std::vector<VkDescriptorPoolSize> poolSizes;

    for (const BindingInfo* bindingInfo : RHIInternal::CollectBindingInfosForSet(desc, setIndex))
    {
        const VkDescriptorType descriptorType = ToVkDescriptorType(bindingInfo->m_Kind);
        const uint32_t descriptorCount = bindingInfo->m_ArrayCount;

        auto it = std::find_if(poolSizes.begin(),
                               poolSizes.end(),
                               [descriptorType](const VkDescriptorPoolSize& poolSize)
                               { return poolSize.type == descriptorType; });
        if (it == poolSizes.end())
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = descriptorType;
            poolSize.descriptorCount = descriptorCount;
            poolSizes.push_back(poolSize);
        }
        else
        {
            it->descriptorCount += descriptorCount;
        }
    }

    VkDescriptorPoolCreateInfo createInfo =
        MakeVkStruct<VkDescriptorPoolCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO>();
    createInfo.maxSets = 1;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.empty() ? nullptr : poolSizes.data();

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    CheckVk(vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool");
    return descriptorPool;
}

VkDescriptorSet AllocateVkDescriptorSet(VkDevice device,
                                        VkDescriptorPool descriptorPool,
                                        const VulkanPipelineLayout& pipelineLayout,
                                        uint32_t setIndex)
{
    const VkDescriptorSetLayout descriptorSetLayout = pipelineLayout.GetVkDescriptorSetLayout(setIndex);
    RTRLAB_ASSERT_MSG(descriptorSetLayout != VK_NULL_HANDLE,
                      "Vulkan ResourceSet allocation requires a valid VkDescriptorSetLayout.");

    VkDescriptorSetAllocateInfo allocateInfo =
        MakeVkStruct<VkDescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO>();
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    CheckVk(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet), "vkAllocateDescriptorSets");
    return descriptorSet;
}

VkPipelineLayout CreateVkPipelineLayout(VkDevice device,
                                        const PipelineLayoutDesc& desc,
                                        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
{
    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(desc.m_PushConstants.size());
    for (const PushConstantRangeDesc& pushConstant : desc.m_PushConstants)
    {
        VkPushConstantRange vkPushConstantRange{};
        vkPushConstantRange.stageFlags = ToVkShaderStageFlags(pushConstant.m_StageMask);
        vkPushConstantRange.offset = pushConstant.m_Offset;
        vkPushConstantRange.size = pushConstant.m_Size;
        pushConstantRanges.push_back(vkPushConstantRange);
    }

    VkPipelineLayoutCreateInfo createInfo =
        MakeVkStruct<VkPipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO>();
    createInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    createInfo.pSetLayouts = descriptorSetLayouts.data();
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    CheckVk(vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    return pipelineLayout;
}
} // namespace VulkanRHI
