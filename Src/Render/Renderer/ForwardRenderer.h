#pragma once

/// @file ForwardRenderer.h
/// @brief Minimal forward-renderer shell for shared demo rendering setup.

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "Core/Util/Base.h"
#include "Render/RHI/RHI.h"
#include "Render/Renderer/FrameGlobals.h"
#include "Render/Renderer/Material.h"
#include "Render/Renderer/RenderObject.h"

namespace Renderer
{
struct ForwardRendererSetIndices
{
    uint32_t m_FrameSet = 0;
    uint32_t m_MaterialSet = 0;
    uint32_t m_ObjectSet = 0;
};

struct ForwardRendererDesc
{
    std::filesystem::path m_ShaderPath;
    Format m_ColorFormat = Format::Unknown;
    Format m_DepthFormat = Format::Unknown;
    uint32_t m_VertexStride = 0;
    uint32_t m_PositionOffset = 0;
    uint32_t m_UVOffset = 0;
    std::string_view m_DebugName = "ForwardRenderer";
};

class ForwardRenderer
{
public:
    void Initialize(Device& device, const ForwardRendererDesc& desc);

    bool IsInitialized() const;
    void Reset();

    Scope<ResourceSet> CreateMaterialSet(Device& device) const;
    Scope<ResourceSet> CreateObjectSet(Device& device) const;
    void UpdateFrameGlobals(const FrameGlobals& frameGlobals) const;
    void UpdateMaterial(const Material& material) const;
    void UpdateRenderObject(const RenderObject& object) const;
    void DrawObject(CommandList& commandList, const RenderObject& object) const;

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
