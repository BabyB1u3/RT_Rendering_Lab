#pragma once

/// @file VulkanPipelineLayout.h
/// @brief Backend-private Vulkan pipeline-layout and vertex-input wrappers.

#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHIPipeline.h"

class VulkanVertexInputLayout final : public VertexInputLayout
{
public:
    VulkanVertexInputLayout(const VertexInputLayoutDesc& desc,
                            std::vector<VkVertexInputBindingDescription>&& bindings,
                            std::vector<VkVertexInputAttributeDescription>&& attributes)
        : m_Desc(desc), m_Bindings(std::move(bindings)), m_Attributes(std::move(attributes))
    {
    }

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }
    const std::vector<VkVertexInputBindingDescription>& GetVkBindings() const { return m_Bindings; }
    const std::vector<VkVertexInputAttributeDescription>& GetVkAttributes() const { return m_Attributes; }

private:
    VertexInputLayoutDesc m_Desc;
    std::vector<VkVertexInputBindingDescription> m_Bindings;
    std::vector<VkVertexInputAttributeDescription> m_Attributes;
};

class VulkanPipelineLayout final : public PipelineLayout
{
public:
    VulkanPipelineLayout(VkDevice device,
                         const PipelineLayoutDesc& desc,
                         VkPipelineLayout pipelineLayout,
                         std::vector<VkDescriptorSetLayout>&& descriptorSetLayouts)
        : m_Device(device),
          m_Desc(desc),
          m_PipelineLayout(pipelineLayout),
          m_DescriptorSetLayouts(std::move(descriptorSetLayouts))
    {
    }

    ~VulkanPipelineLayout() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        if (m_PipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

        for (VkDescriptorSetLayout descriptorSetLayout : m_DescriptorSetLayouts)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(m_Device, descriptorSetLayout, nullptr);
        }
    }

    const PipelineLayoutDesc& GetDesc() const override { return m_Desc; }
    VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }
    VkDescriptorSetLayout GetVkDescriptorSetLayout(uint32_t setIndex) const
    {
        RTRLAB_ASSERT_MSG(setIndex < m_DescriptorSetLayouts.size(),
                          "Vulkan descriptor-set layout index is out of range.");
        return m_DescriptorSetLayouts[setIndex];
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    PipelineLayoutDesc m_Desc;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
};

inline VulkanPipelineLayout& GetVulkanPipelineLayout(PipelineLayout* pipelineLayout)
{
    auto* vulkanPipelineLayout = dynamic_cast<VulkanPipelineLayout*>(pipelineLayout);
    RTRLAB_ASSERT_MSG(vulkanPipelineLayout != nullptr, "PipelineLayout is not owned by the Vulkan backend.");
    return *vulkanPipelineLayout;
}

inline const VulkanVertexInputLayout& GetVulkanVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* vulkanVertexInputLayout = dynamic_cast<VulkanVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(vulkanVertexInputLayout != nullptr, "VertexInputLayout is not owned by the Vulkan backend.");
    return *vulkanVertexInputLayout;
}
