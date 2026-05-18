#pragma once

/// @file ForwardRenderer.h
/// @brief Minimal forward-renderer shell for shared demo rendering setup.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Render/RHI/RHI.h"

namespace Renderer
{
struct ForwardRendererSetIndices
{
    uint32_t m_FrameSet = 0;
    uint32_t m_MaterialSet = 0;
    uint32_t m_ObjectSet = 0;
};

class ForwardRenderer
{
public:
    bool IsInitialized() const;
    void Reset();

    ShaderProgram* GetShaderProgram() const { return m_ShaderProgram.get(); }
    PipelineLayout* GetPipelineLayout() const { return m_PipelineLayout.get(); }
    ResourceSet* GetFrameSet() const { return m_FrameSet.get(); }
    VertexInputLayout* GetVertexInputLayout() const { return m_VertexInputLayout.get(); }
    GraphicsPipeline* GetGraphicsPipeline() const { return m_GraphicsPipeline.get(); }
    const ForwardRendererSetIndices& GetSetIndices() const { return m_SetIndices; }

private:
    Scope<ShaderProgram> m_ShaderProgram;
    Scope<PipelineLayout> m_PipelineLayout;
    Scope<ResourceSet> m_FrameSet;
    Scope<VertexInputLayout> m_VertexInputLayout;
    Scope<GraphicsPipeline> m_GraphicsPipeline;
    ForwardRendererSetIndices m_SetIndices;
};
} // namespace Renderer
