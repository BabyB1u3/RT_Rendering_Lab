#pragma once

/// @file VulkanGraphicsPipeline.h
/// @brief Backend-private Vulkan graphics-pipeline wrapper.

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanPipelineLayout.h"
#include "Render/RHI/RHIPipeline.h"

class VulkanGraphicsPipeline final : public GraphicsPipeline
{
public:
    VulkanGraphicsPipeline(VkDevice device,
                           const GraphicsPipelineDesc& desc,
                           VulkanPipelineLayout* pipelineLayout,
                           VkPipeline pipeline)
        : m_Device(device), m_Desc(desc), m_PipelineLayout(pipelineLayout), m_Pipeline(pipeline)
    {
    }

    ~VulkanGraphicsPipeline() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        if (m_Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    VkPipeline GetVkPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetVkPipelineLayout() const
    {
        return m_PipelineLayout != nullptr ? m_PipelineLayout->GetVkPipelineLayout() : VK_NULL_HANDLE;
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    GraphicsPipelineDesc m_Desc;
    VulkanPipelineLayout* m_PipelineLayout = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

inline const VulkanGraphicsPipeline& GetVulkanGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* vulkanGraphicsPipeline = dynamic_cast<VulkanGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(vulkanGraphicsPipeline != nullptr, "GraphicsPipeline is not owned by the Vulkan backend.");
    return *vulkanGraphicsPipeline;
}
