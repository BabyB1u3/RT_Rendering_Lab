#pragma once

/// @file MetalShaderProgram.h
/// @brief Backend-private Metal shader-program wrapper.

#include <algorithm>
#include <vector>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/RHIPipeline.h"

class MetalShaderProgram final : public ShaderProgram
{
public:
    struct StageFunction
    {
        ShaderStage m_Stage = ShaderStage::None;
        id<MTLFunction> m_Function = nil;
    };

    MetalShaderProgram(const CompiledShaderProgramDesc& desc,
                       std::vector<StageFunction>&& functions,
                       std::vector<MetalRHI::MetalSetBindingPlan>&& setBindingPlans)
        : m_Reflection(desc.m_Reflection),
          m_Functions(std::move(functions)),
          m_SetBindingPlans(std::move(setBindingPlans))
    {
    }

    ~MetalShaderProgram() override
    {
        for (const StageFunction& function : m_Functions)
        {
            if (function.m_Function != nil)
                [function.m_Function release];
        }
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }

    id<MTLFunction> FindStage(ShaderStage stage) const
    {
        const auto it = std::find_if(m_Functions.begin(),
                                     m_Functions.end(),
                                     [stage](const StageFunction& function) { return function.m_Stage == stage; });
        return it != m_Functions.end() ? it->m_Function : nil;
    }

    const MetalRHI::MetalStageBindingPlan* FindStageBindingPlan(uint32_t setIndex, ShaderStage stage) const;
    const std::vector<MetalRHI::MetalSetBindingPlan>& GetSetBindingPlans() const { return m_SetBindingPlans; }

private:
    ShaderReflectionData m_Reflection;
    std::vector<StageFunction> m_Functions;
    std::vector<MetalRHI::MetalSetBindingPlan> m_SetBindingPlans;
};

const MetalShaderProgram& GetMetalShaderProgram(ShaderProgram* shaderProgram);
