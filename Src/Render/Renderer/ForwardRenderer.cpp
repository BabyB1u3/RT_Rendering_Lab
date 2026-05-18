#include "Render/Renderer/ForwardRenderer.h"

namespace Renderer
{
bool ForwardRenderer::IsInitialized() const
{
    return m_ShaderProgram != nullptr && m_PipelineLayout != nullptr && m_FrameSet != nullptr &&
           m_VertexInputLayout != nullptr && m_GraphicsPipeline != nullptr;
}

void ForwardRenderer::Reset()
{
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_FrameSet.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
    m_SetIndices = {};
}
} // namespace Renderer
