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

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexBuffer;
    Scope<ShaderProgram> m_ShaderProgram;
    Scope<PipelineLayout> m_PipelineLayout;
    Scope<VertexInputLayout> m_VertexInputLayout;
    Scope<GraphicsPipeline> m_GraphicsPipeline;
};
