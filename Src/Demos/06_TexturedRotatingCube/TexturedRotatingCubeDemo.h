#pragma once

/// @file TexturedRotatingCubeDemo.h
/// @brief Tutorial 06: draw a continuously rotating textured cube.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Demos/DemoBase.h"
#include "Render/RHI/RHI.h"

class TexturedRotatingCubeDemo : public DemoBase
{
public:
    TexturedRotatingCubeDemo(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void CreateCubeResources();
    void CreateDepthResources();
    void UploadPendingResources(CommandList& commandList, ResourceStateTracker& resourceStateTracker);
    void UpdateShaderParameters();
    void ForgetTrackedResources();

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
    float m_RotationSeconds = 0.0f;

    Scope<Buffer> m_VertexUploadBuffer;
    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexUploadBuffer;
    Scope<Buffer> m_IndexBuffer;
    Scope<Buffer> m_TextureUploadBuffer;
    bool m_GeometryUploadPending = false;
    bool m_TextureUploadPending = false;

    Scope<Texture> m_Texture;
    Scope<TextureView> m_TextureView;
    Scope<Sampler> m_Sampler;
    Scope<Texture> m_DepthTexture;
    Scope<TextureView> m_DepthView;

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
