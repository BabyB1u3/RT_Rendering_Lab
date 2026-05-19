#include "Render/RHI/Backends/Metal/Pipeline/MetalShaderProgram.h"

#include "Core/Diagnostics/Assert/Assert.h"

const MetalRHI::MetalStageBindingPlan* MetalShaderProgram::FindStageBindingPlan(uint32_t setIndex,
                                                                                ShaderStage stage) const
{
    const auto setIt =
        std::find_if(m_SetBindingPlans.begin(),
                     m_SetBindingPlans.end(),
                     [setIndex](const MetalRHI::MetalSetBindingPlan& plan) { return plan.m_SetIndex == setIndex; });
    if (setIt == m_SetBindingPlans.end())
        return nullptr;

    const auto stageIt =
        std::find_if(setIt->m_StagePlans.begin(),
                     setIt->m_StagePlans.end(),
                     [stage](const MetalRHI::MetalStageBindingPlan& stagePlan) { return stagePlan.m_Stage == stage; });
    return stageIt != setIt->m_StagePlans.end() ? &(*stageIt) : nullptr;
}

const MetalShaderProgram& GetMetalShaderProgram(ShaderProgram* shaderProgram)
{
    auto* metalShaderProgram = dynamic_cast<MetalShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(metalShaderProgram != nullptr, "GraphicsPipeline requires a Metal shader program.");
    return *metalShaderProgram;
}
