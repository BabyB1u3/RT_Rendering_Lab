#pragma once

/// @file VulkanSampler.h
/// @brief Backend-private Vulkan sampler wrapper.

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHIResources.h"

class VulkanSampler final : public Sampler
{
public:
    VulkanSampler(VkDevice device, VkSampler sampler, const SamplerDesc& desc)
        : m_Device(device), m_Sampler(sampler), m_Desc(desc)
    {
    }

    ~VulkanSampler() override
    {
        if (m_Device != VK_NULL_HANDLE && m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_Device, m_Sampler, nullptr);
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    VkSampler GetVkSampler() const { return m_Sampler; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    SamplerDesc m_Desc;
};

inline VulkanSampler& GetVulkanSampler(Sampler* sampler)
{
    auto* vulkanSampler = dynamic_cast<VulkanSampler*>(sampler);
    RTRLAB_ASSERT_MSG(vulkanSampler != nullptr, "Sampler is not owned by the Vulkan backend.");
    return *vulkanSampler;
}
