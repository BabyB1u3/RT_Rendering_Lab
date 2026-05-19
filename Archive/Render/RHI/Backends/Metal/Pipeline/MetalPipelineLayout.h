#pragma once

/// @file MetalPipelineLayout.h
/// @brief Backend-private Metal pipeline-layout and vertex-input wrappers.

#include <algorithm>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/RHIPipeline.h"

class MetalPipelineLayout final : public PipelineLayout
{
public:
    explicit MetalPipelineLayout(const PipelineLayoutDesc& desc)
        : m_Desc(desc),
          m_SetBindingPlans(MetalRHI::BuildMetalSetBindingPlans(desc)),
          m_VertexBufferSlotBase(MetalRHI::ComputeVertexBufferSlotBase(m_SetBindingPlans))
    {
    }

    const PipelineLayoutDesc& GetDesc() const override { return m_Desc; }

    const MetalRHI::MetalSetBindingPlan& GetSetBindingPlan(uint32_t setIndex) const
    {
        const auto it =
            std::find_if(m_SetBindingPlans.begin(),
                         m_SetBindingPlans.end(),
                         [setIndex](const MetalRHI::MetalSetBindingPlan& plan) { return plan.m_SetIndex == setIndex; });
        RTRLAB_ASSERTF(it != m_SetBindingPlans.end(),
                       "Metal pipeline layout does not contain a binding plan for set {}.",
                       setIndex);
        return *it;
    }

    const MetalRHI::MetalStageBindingPlan* FindStageBindingPlan(uint32_t setIndex, ShaderStage stage) const;
    const std::vector<MetalRHI::MetalSetBindingPlan>& GetSetBindingPlans() const { return m_SetBindingPlans; }
    uint32_t GetVertexBufferSlotBase() const { return m_VertexBufferSlotBase; }

private:
    PipelineLayoutDesc m_Desc;
    std::vector<MetalRHI::MetalSetBindingPlan> m_SetBindingPlans;
    uint32_t m_VertexBufferSlotBase = 0;
};

class MetalVertexInputLayout final : public VertexInputLayout
{
public:
    explicit MetalVertexInputLayout(const VertexInputLayoutDesc& desc) : m_Desc(desc) {}

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }

private:
    VertexInputLayoutDesc m_Desc;
};

MetalPipelineLayout& GetMetalPipelineLayout(PipelineLayout* pipelineLayout);
const MetalVertexInputLayout& GetMetalVertexInputLayout(VertexInputLayout* vertexInputLayout);
