#include "Render/RHI/Backends/Metal/Pipeline/MetalPipelineLayout.h"

const MetalRHI::MetalStageBindingPlan* MetalPipelineLayout::FindStageBindingPlan(uint32_t setIndex,
                                                                                 ShaderStage stage) const
{
    const MetalRHI::MetalSetBindingPlan& setPlan = GetSetBindingPlan(setIndex);
    const auto it =
        std::find_if(setPlan.m_StagePlans.begin(),
                     setPlan.m_StagePlans.end(),
                     [stage](const MetalRHI::MetalStageBindingPlan& stagePlan) { return stagePlan.m_Stage == stage; });
    return it != setPlan.m_StagePlans.end() ? &(*it) : nullptr;
}

MetalPipelineLayout& GetMetalPipelineLayout(PipelineLayout* pipelineLayout)
{
    auto* metalPipelineLayout = dynamic_cast<MetalPipelineLayout*>(pipelineLayout);
    RTRLAB_ASSERT_MSG(metalPipelineLayout != nullptr, "PipelineLayout is not owned by the Metal backend.");
    return *metalPipelineLayout;
}

const MetalVertexInputLayout& GetMetalVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* metalVertexInputLayout = dynamic_cast<MetalVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(metalVertexInputLayout != nullptr, "GraphicsPipeline requires a Metal vertex input layout.");
    return *metalVertexInputLayout;
}
