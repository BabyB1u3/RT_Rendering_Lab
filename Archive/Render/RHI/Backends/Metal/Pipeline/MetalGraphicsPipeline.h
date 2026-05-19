#pragma once

/// @file MetalGraphicsPipeline.h
/// @brief Backend-private Metal graphics-pipeline wrapper.

#include <algorithm>
#include <vector>

#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/RHIPipeline.h"

struct MetalStageArgumentEncoderEntry
{
    uint32_t m_SetIndex = 0;
    ShaderStage m_Stage = ShaderStage::None;
    uint32_t m_Slot = 0;
    id<MTLArgumentEncoder> m_Encoder = nil;
};

class MetalGraphicsPipeline final : public GraphicsPipeline
{
public:
    MetalGraphicsPipeline(id<MTLRenderPipelineState> pipelineState,
                          id<MTLDepthStencilState> depthStencilState,
                          const GraphicsPipelineDesc& desc,
                          uint32_t vertexBufferSlotBase,
                          std::vector<MetalStageArgumentEncoderEntry>&& argumentEncoders)
        : m_PipelineState([pipelineState retain]),
          m_DepthStencilState([depthStencilState retain]),
          m_Desc(desc),
          m_VertexBufferSlotBase(vertexBufferSlotBase),
          m_ArgumentEncoders(std::move(argumentEncoders))
    {
    }

    ~MetalGraphicsPipeline() override
    {
        if (m_PipelineState != nil)
        {
            [m_PipelineState release];
            m_PipelineState = nil;
        }

        if (m_DepthStencilState != nil)
        {
            [m_DepthStencilState release];
            m_DepthStencilState = nil;
        }

        for (const MetalStageArgumentEncoderEntry& entry : m_ArgumentEncoders)
        {
            if (entry.m_Encoder != nil)
                [entry.m_Encoder release];
        }
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    id<MTLRenderPipelineState> GetPipelineState() const { return m_PipelineState; }
    id<MTLDepthStencilState> GetDepthStencilState() const { return m_DepthStencilState; }
    uint32_t GetVertexBufferSlotBase() const { return m_VertexBufferSlotBase; }
    const MetalStageArgumentEncoderEntry* FindArgumentEncoderEntry(uint32_t setIndex, ShaderStage stage) const
    {
        const auto it = std::find_if(m_ArgumentEncoders.begin(),
                                     m_ArgumentEncoders.end(),
                                     [setIndex, stage](const MetalStageArgumentEncoderEntry& entry)
                                     { return entry.m_SetIndex == setIndex && entry.m_Stage == stage; });
        return it != m_ArgumentEncoders.end() ? &(*it) : nullptr;
    }

private:
    id<MTLRenderPipelineState> m_PipelineState = nil;
    id<MTLDepthStencilState> m_DepthStencilState = nil;
    GraphicsPipelineDesc m_Desc;
    uint32_t m_VertexBufferSlotBase = 0;
    std::vector<MetalStageArgumentEncoderEntry> m_ArgumentEncoders;
};

const MetalGraphicsPipeline& GetMetalGraphicsPipeline(GraphicsPipeline* graphicsPipeline);
