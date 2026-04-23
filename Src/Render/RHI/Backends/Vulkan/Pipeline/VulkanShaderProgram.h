#pragma once

/// @file VulkanShaderProgram.h
/// @brief Backend-private Vulkan shader-program wrapper owning per-stage
/// VkShaderModule handles plus shader reflection.

#include <algorithm>
#include <string>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHIPipeline.h"
#include "Render/Shader/ShaderTypes.h"

class VulkanShaderProgram final : public ShaderProgram
{
public:
    struct StageModule
    {
        ShaderStage m_Stage = ShaderStage::None;
        std::string m_EntryPoint;
        VkShaderModule m_Module = VK_NULL_HANDLE;
    };

    VulkanShaderProgram(VkDevice device, const CompiledShaderProgramDesc& desc, std::vector<StageModule>&& modules)
        : m_Device(device), m_Reflection(desc.m_Reflection), m_Modules(std::move(modules))
    {
    }

    ~VulkanShaderProgram() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        for (const StageModule& module : m_Modules)
        {
            if (module.m_Module != VK_NULL_HANDLE)
                vkDestroyShaderModule(m_Device, module.m_Module, nullptr);
        }
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }

    const StageModule* FindStage(ShaderStage stage) const
    {
        const auto it = std::find_if(
            m_Modules.begin(), m_Modules.end(), [stage](const StageModule& module) { return module.m_Stage == stage; });
        return it != m_Modules.end() ? &(*it) : nullptr;
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    ShaderReflectionData m_Reflection;
    std::vector<StageModule> m_Modules;
};

inline const VulkanShaderProgram& GetVulkanShaderProgram(ShaderProgram* shaderProgram)
{
    auto* vulkanShaderProgram = dynamic_cast<VulkanShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(vulkanShaderProgram != nullptr, "GraphicsPipeline requires a Vulkan shader program.");
    return *vulkanShaderProgram;
}
