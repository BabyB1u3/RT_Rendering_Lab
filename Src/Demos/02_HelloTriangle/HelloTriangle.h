#pragma once

/// @file HelloTriangle.h
/// @brief Tutorial 02: validate the first indexed draw path on Vulkan.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Demos/DemoBase.h"
#include "Render/RHI/RHI.h"

class HelloTriangle : public DemoBase
{
public:
    HelloTriangle(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void CreateTriangleResources();
    void UploadTriangleGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker);
    void ForgetTrackedBuffers();
    void UpdateAnimatedParameters();

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Scope<Buffer> m_VertexUploadBuffer;
    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexUploadBuffer;
    Scope<Buffer> m_IndexBuffer;
    bool m_GeometryUploadPending = false;
    Scope<ShaderProgram> m_ShaderProgram;
    Scope<PipelineLayout> m_PipelineLayout;
    Scope<ResourceSet> m_FrameSet;
    uint32_t m_FrameSetIndex = 0;
    Scope<ResourceSet> m_MaterialSet;
    uint32_t m_MaterialSetIndex = 1;
    Scope<ResourceSet> m_ObjectSet;
    uint32_t m_ObjectSetIndex = 2;
    Scope<VertexInputLayout> m_VertexInputLayout;
    Scope<GraphicsPipeline> m_GraphicsPipeline;
};
