#pragma once

/// @file HelloOffscreen.h
/// @brief Tutorial 04: validate offscreen rendering and sampled resolve to the swapchain.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Demos/DemoBase.h"
#include "Render/RHI/RHI.h"

class HelloOffscreen : public DemoBase
{
public:
    HelloOffscreen(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void CreateOffscreenDemoResources();
    void UploadPendingGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker);
    void ForgetTrackedResources();

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Scope<Buffer> m_VertexUploadBuffer;
    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexUploadBuffer;
    Scope<Buffer> m_IndexBuffer;
    bool m_GeometryUploadPending = false;

    Scope<Texture> m_OffscreenColorTexture;
    Scope<TextureView> m_OffscreenColorView;
    Scope<Sampler> m_OffscreenSampler;

    Scope<ShaderProgram> m_OffscreenShaderProgram;
    Scope<PipelineLayout> m_OffscreenPipelineLayout;
    Scope<GraphicsPipeline> m_OffscreenPipeline;

    Scope<ShaderProgram> m_CompositeShaderProgram;
    Scope<PipelineLayout> m_CompositePipelineLayout;
    Scope<ResourceSet> m_CompositeTextureSet;
    uint32_t m_CompositeTextureSetIndex = 0;
    Scope<GraphicsPipeline> m_CompositePipeline;

    Scope<VertexInputLayout> m_VertexInputLayout;
};
