#pragma once

/// @file HelloTexture.h
/// @brief Tutorial 03: validate staged texture upload and sampled rendering.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Demos/DemoBase.h"
#include "Render/RHI/RHI.h"

class HelloTexture : public DemoBase
{
public:
    HelloTexture(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void CreateTextureDemoResources();
    void UploadPendingResources(CommandList& commandList, ResourceStateTracker& resourceStateTracker);
    void ForgetTrackedResources();

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Scope<Buffer> m_VertexUploadBuffer;
    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexUploadBuffer;
    Scope<Buffer> m_IndexBuffer;
    Scope<Buffer> m_TextureUploadBuffer;
    Scope<Texture> m_Texture;
    Scope<TextureView> m_TextureView;
    Scope<Sampler> m_Sampler;
    bool m_GeometryUploadPending = false;
    bool m_TextureUploadPending = false;

    Scope<ShaderProgram> m_ShaderProgram;
    Scope<PipelineLayout> m_PipelineLayout;
    Scope<ResourceSet> m_TextureSet;
    uint32_t m_TextureSetIndex = 0;
    Scope<VertexInputLayout> m_VertexInputLayout;
    Scope<GraphicsPipeline> m_GraphicsPipeline;
};
